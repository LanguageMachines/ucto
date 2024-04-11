/*
  Copyright (c) 2006 - 2024
  CLST - Radboud University
  ILK  - Tilburg University

  This file is part of Ucto

  Ucto is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  Ucto is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/ucto/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Unicode.h"
#include "ucto/my_textcat.h"
#include "ucto/setting.h"
#include "ucto/tokenize.h"
#include <unistd.h>

using namespace std;
using namespace Tokenizer;
using TiCC::operator<<;

string fix_639_1( const string& language ){
  string result = language;
  // support some backward compatability to old ISO 639-1 codes
  if ( language == "nl" ){
    result = "nld";
  }
  else if ( language == "de" ){
    result = "deu";
  }
  else if ( language == "fr" ){
    result = "fra";
  }
  else if ( language == "pt" ){
    result = "por";
  }
  else if ( language == "es" ){
    result = "spa";
  }
  else if ( language == "fy" ){
    result = "fry";
  }
  else if ( language == "se" ){
    result = "swe";
  }
  else if ( language == "en" ){
    result = "eng";
  }
  else if ( language == "it" ){
    result = "ita";
  }
  else if ( language == "ru" ){
    result = "rus";
  }
  else if ( language == "tr" ){
    result = "tur";
  }
  return result;
}

void usage(){
  set<string> languages = Setting::installed_languages();
  cerr << "Usage: " << endl;
  cerr << "\tucto [[options]] [input-file] [[output-file]]"  << endl
       << "Options:" << endl
       << "\t-c <configfile>   - Explicitly specify a configuration file" << endl
       << "\t-B                - Run in batch mode. Requires at least -O" << endl
       << "\t-I <inpdir>       - the input directory to find input files (batch mode only) " << endl
       << "\t-O <outdir>       - the output directory to stored results. (required for batch mode)" << endl
       << "\t-d <value>        - set debug level" << endl
       << "\t-e <string>       - set input encoding (default UTF8)" << endl
       << "\t-N <string>       - set output normalization (default NFC)" << endl
       << "\t--filter=[YES|NO] - Disable filtering of special characters" << endl
       << "\t-h or --help      - this message" << endl
       << "\t-L <language>     - Automatically selects a configuration file by language code." << endl
       << "\t                  - Available Languages:" << endl
       << "\t                    ";
  for( const auto& l : languages ){
    cerr << l << ",";
  }
  cerr << endl;
  cerr << "\t-l                - Convert to all lowercase" << endl
       << "\t-u                - Convert to all uppercase" << endl
       << "\t-n                - One sentence per line (output)" << endl
       << "\t-m                - One sentence per line (input)" << endl
       << "\t-v                - Verbose mode" << endl
       << "\t-s <string>       - End-of-Sentence marker (default: <utt>)" << endl
       << "\t--passthru        - Don't tokenize, but perform input decoding and simple token role detection" << endl
       << "\t--normalize=<class1>,class2>,... " << endl
       << "\t                  - For class1, class2, etc. output the class tokens instead of the tokens itself." << endl
       << "\t-T or --textredundancy=[full|minimal|none]  - set text redundancy level for text nodes in FoLiA output: " << endl
       << "\t                    'full' - add text to all levels: <p> <s> <w> etc." << endl
       << "\t                    'minimal' - don't introduce text on higher levels, but retain what is already there." << endl
       << "\t                    'none' - only introduce text on <w>, AND remove all text from higher levels" << endl
       << "\t--allow-word-corrections   - allow tokenization of FoLiA Word elements." << endl
       << "\t--ignore-tag-hints - Do NOT use tag=\"token\" hints from the FoLiA input. (default is to use them)" << endl
       << "\t--filterpunct      - remove all punctuation from the output" << endl
       << "\t--uselanguages=<lang1,lang2,..langn> - Using FoLiA input, only tokenize strings in these languages. Default = 'lang1'" << endl
       << "\t--detectlanguages=<lang1,lang2,..langn> - try to assign a language to each line of text input. Default = 'lang1'" << endl
       << "\t\tFor both uselanguages and detectlanguages, you can use the special" << endl
       << "\t\tlanguage code `und`. This ensures there is NO default language, but" << endl
       << "\t\tany language that is NOT in the list will remain unanalyzed." << endl
       << "\t--add-tokens='file' - add additional tokens to the [TOKENS] of the" << endl
       << "\t                    default language. TOKENS are always kept intact." << endl
       << "\t-P                - Disable paragraph detection" << endl
       << "\t-Q                - Enable quote detection (experimental)" << endl
       << "\t-V or --version   - Show version information" << endl
       << "\t-F                - Input file is in FoLiA XML. All untokenized sentences will be tokenized." << endl
       << "\t                    -F is automatically set when inputfile has extension '.xml'" << endl
       << "\t                    In batch mode this assures that only files with '.xml' extensions are processed" << endl
       << "\t-X                - Output FoLiA XML, use the Document ID specified with --id=" << endl
       << "\t                    This option is automatically set when inputfile has extension '.xml'" << endl
       << "\t                    In batch mode, this forces all output to be FoLiA XML. the Document ID is autogenerated." << endl
       << "\t--id <DocID>      - use the specified Document ID to label the FoLia doc. (not valid in batch mode)" << endl
       << "\t--inputclass <class>  - use the specified class to search text in the FoLiA doc.(default is 'current')" << endl
       << "\t--outputclass <class> - use the specified class to output text in the FoLiA doc. (default is 'current')" << endl
       << "\t--textclass <class>   - use the specified class for both input and output of text in the FoLiA doc. (default is 'current'). Implies --filter=NO." << endl
       << "\t--copyclass           - when no new tokens are added, copy the text in the inputclass to the outputclass (default=NO)" << endl
       << "\t--separators=\"seps\" - use the specified 'seps` as the main separators. (default is '+', meaning ALL spacing)" << endl
       << "\t                    '+' : the default, ALL spacing characters" << endl
       << "\t                    '+<UTF8-string>' : use ALL spacing PLUS all characters from the UTF8 string" << endl
       << "\t                      that string may again include a '+', to add it too" << endl
       << "\t                    '-+' : special case to use only the '+' as separator" << endl;
}

void usage_small(){
  cerr << "Usage: " << endl;
  cerr << "\tucto [[options]] [input-file] [[output-file]]"  << endl
       << "\t -h or --help for more info" << endl;
}

class runtime_opts {
public:
  runtime_opts();
  void fill( TiCC::CL_Options& );
  pair<istream *, ostream *> determine_io( const pair<string,string>& );
  vector<pair<string,string>> create_file_list();
  pair<string,string> create_io_pair( const string&, const string& );
  void check_xmlin_opt();
  void check_xmlout_opt();
  int debug;
  bool batchmode;
  bool tolowercase;
  bool touppercase;
  bool sentenceperlineoutput;
  bool sentenceperlineinput;
  bool paragraphdetection;
  bool quotedetection;
  bool do_language_detect;
  bool use_lang;
  bool detect_lang;
  bool dofiltering;
  bool dopunctfilter;
  bool xmlin;
  bool force_xmlin;
  bool xmlout;
  bool force_xmlout;
  bool verbose;
  bool docorrectwords;
  bool do_und_lang;
  bool pass_thru;
  bool ignore_tags;
  bool sentencesplit;
  bool copyclass;
  string redundancy;
  string utt_marker;
  string docid;
  string normalization;
  string inputEncoding;
  string inputclass;
  string outputclass;
  string ifile;
  string ofile;
  string input_dir;
  string output_dir;
  string c_file;
  string norm_set_string;
  string add_tokens;
  string command_line;
  string separators;
  vector<string> language_list;
  vector<string> input_files;
  vector<pair<string,string>> file_list;
};

runtime_opts::runtime_opts():
  debug(0),
  batchmode(false),
  tolowercase(false),
  touppercase(false),
  sentenceperlineoutput(false),
  sentenceperlineinput(false),
  paragraphdetection(true),
  quotedetection(false),
  do_language_detect(false),
  use_lang(false),
  detect_lang(false),
  dofiltering(true),
  dopunctfilter(false),
  xmlin(false),
  force_xmlin(false),
  xmlout(false),
  force_xmlout(false),
  verbose(false),
  docorrectwords(false),
  do_und_lang(false),
  pass_thru(false),
  ignore_tags(false),
  sentencesplit(false),
  copyclass(false),
  redundancy("minimal"),
  utt_marker("<utt>"),
  normalization("NFC"),
  inputEncoding("UTF-8"),
  inputclass ("current"),
  outputclass("current"),
  command_line("ucto"),
  separators("+")
{}

void runtime_opts::check_xmlin_opt(){
  if ( use_lang && !xmlin ){
    throw TiCC::OptionError( "--uselanguages is only valid for FoLiA input" );
  }
  if ( docorrectwords && !xmlin ){
    throw TiCC::OptionError( "--allow-word-corrections is only valid for FoLiA input" );
  }
  if ( xmlin && outputclass.empty() ){
    if ( dopunctfilter ){
      throw TiCC::OptionError( "--outputclass required for --filterpunct on FoLiA input ");
    }
    if ( touppercase ){
      throw TiCC::OptionError( "--outputclass required for -u on FoLiA input ");
    }
    if ( tolowercase ){
      throw TiCC::OptionError( "--outputclass required for -l on FoLiA input ");
    }
  }
}

void runtime_opts::check_xmlout_opt(){
  if ( sentencesplit
       && xmlout ){
    throw TiCC::OptionError( "conflicting options --split and -X" );
  }
}

void runtime_opts::fill( TiCC::CL_Options& Opts ){
  Opts.extract('e', inputEncoding );
  batchmode = Opts.extract( 'B' );
  dopunctfilter = Opts.extract( "filterpunct" );
  docorrectwords = Opts.extract( "allow-word-corrections" );
  paragraphdetection = !Opts.extract( 'P' );
  xmlin = Opts.extract( 'F' );
  force_xmlin = xmlin;
  quotedetection = Opts.extract( 'Q' );
  Opts.extract( 's', utt_marker );
  touppercase = Opts.extract( 'u' );
  tolowercase = Opts.extract( 'l' );
  sentencesplit = Opts.extract( "split" );
  sentenceperlineoutput = Opts.extract( 'n' );
  sentenceperlineinput = Opts.extract( 'm' );
  Opts.extract( 'T', redundancy );
  Opts.extract( "textredundancy", redundancy );
  if ( redundancy != "full"
       && redundancy != "minimal"
       && redundancy != "none" ){
    throw TiCC::OptionError( "unknown textredundancy level: " + redundancy );
  }
  Opts.extract( 'N', normalization );
  verbose = Opts.extract( 'v' );
  Opts.extract( 'I', input_dir );
  if ( !input_dir.empty()
       && input_dir.back() != '/' ){
    input_dir += "/";
    if ( !TiCC::isDir(input_dir) ){
      throw TiCC::OptionError( "unable to access '" + input_dir + "'" );
    }
  }
  // cerr << "INPUT DIR = " << input_dir << endl;
  if ( !batchmode
       && !input_dir.empty() ){
    throw TiCC::OptionError( "-I option requires batch option (-B) too." );
  }
  Opts.extract( 'O', output_dir );
  if ( !output_dir.empty()
       && output_dir.back() != '/' ){
    output_dir += "/";
    if ( !TiCC::createPath(output_dir) ){
      throw TiCC::OptionError( "unable to write '" + output_dir + "'" );
    }
  }
  if ( !input_dir.empty()
       && output_dir == input_dir ){
    throw TiCC::OptionError( "input dir equals output dir, asking for trouble" );
  }
  if ( batchmode
       && output_dir.empty() ){
    throw TiCC::OptionError( "batch mode requires an output dir (-O option)" );
  }
  //  cerr << "OUTPUT DIR = " << output_dir << endl;
  string textclass;
  Opts.extract( "textclass", textclass );
  Opts.extract( "inputclass", inputclass );
  Opts.extract( "outputclass", outputclass );
  if ( !textclass.empty() ){
    if ( inputclass != "current" ){
      throw TiCC::OptionError( "--textclass conflicts with --inputclass" );
    }
    if ( outputclass != "current" ){
      throw TiCC::OptionError( "--textclass conflicts with --outputclass");
    }
    inputclass = textclass;
    outputclass = textclass;
  }
  copyclass = Opts.extract( "copyclass" );
  if ( copyclass
       && inputclass == outputclass ){
    throw TiCC::OptionError( "--copyclass impossible. "
			     "--inputclass equals --outputclass" );
  }
  if ( Opts.extract( 'f' ) ){
    throw TiCC::OptionError( "ucto: The -f option not supported "
			     "Please use --filter=NO instead" );
  }
  Opts.extract( "add-tokens", add_tokens );
  string value;
  if ( Opts.extract( "filter", value ) ){
    bool result;
    if ( !TiCC::stringTo( value, result ) ){
      throw TiCC::OptionError( "illegal value for '--filter' option. (boolean expected)" );
    }
    dofiltering = result;
  }
  if ( dofiltering
       && xmlin
       && outputclass == inputclass
       && !docorrectwords ){
    // we cannot mangle the original inputclass, so disable filtering
    cerr << "ucto: --filter=NO is automatically set. inputclass equals outputclass!"
	 << endl;
    dofiltering = false;
  }
  ignore_tags = Opts.extract( "ignore-tag-hints" );
  pass_thru = Opts.extract( "passthru" );
  Opts.extract("normalize", norm_set_string );
  Opts.extract( "separators", separators );
  if ( Opts.extract( 'x', docid ) ){
    throw TiCC::OptionError( "ucto: The option '-x ID' is removed. "
			     "Please use '-X' and '--id=ID' instead" );
  }
  else {
    xmlout = Opts.extract( 'X' );
    Opts.extract( "id", docid );
    if ( batchmode ){
      if ( !docid.empty() ){
	throw TiCC::OptionError( "ucto: The option '--id' is not allowed "
				 "in batchmode. Document id's are generated" );
      }
      else {
	force_xmlout = xmlout;
      }
    }
  }
  if ( Opts.extract('d', value ) ){
    if ( !TiCC::stringTo(value,debug) ){
      throw TiCC::OptionError( "invalid value for -d: " + value );
    }
  }
  use_lang = Opts.is_present( "uselanguages" );
  detect_lang = Opts.is_present( "detectlanguages" );
  if ( detect_lang && use_lang ){
    throw TiCC::OptionError( "--detectlanguages and --uselanguages options conflict. Use only one of these." );
  }
  if ( use_lang && pass_thru ){
    throw TiCC::OptionError( "--passtru an --uselanguages options conflict. Use only one of these." );
  }
  if ( detect_lang && pass_thru ){
    throw TiCC::OptionError( "--passtru an --detectlanguages options conflict. Use only one of these." );
  }
  if ( Opts.is_present('L') ) {
    if ( pass_thru ){
      throw TiCC::OptionError( "--passtru an -L options conflict. Use only one of these." );
    }
    if ( Opts.is_present('c') ){
      throw TiCC::OptionError( "-L and -c options conflict. Use only one of these." );
    }
    else if ( detect_lang ){
      throw TiCC::OptionError( "-L and --detectlanguages options conflict. Use only one of these." );
    }
    else if ( use_lang ) {
      throw TiCC::OptionError( "-L and --uselanguages options conflict. Use only one of these." );
    }
  }
  else if ( Opts.is_present( 'c' ) ){
    if ( detect_lang ){
      throw TiCC::OptionError( "-c and --detectlanguages options conflict. Use only one of these" );
    }
    else if ( use_lang ){
      throw TiCC::OptionError( "-c and --uselanguages options conflict. Use only one of these." );
    }
  }
  Opts.extract( 'c', c_file );
  if ( !pass_thru ){
    set<string> available_languages = Setting::installed_languages();
    if ( c_file.empty()
	 && available_languages.empty() ){
      string mess = "ucto: The uctodata package seems not to be installed.\n"
	"ucto: Installing uctodata is a prerequisite.";
      throw TiCC::OptionError( mess );
    }
    string languages;
    Opts.extract( "detectlanguages", languages );
    if ( languages.empty() ){
      Opts.extract( "uselanguages", languages );
    }
    else {
      do_language_detect = true;
    }
    if ( !languages.empty() ){
      language_list = TiCC::split_at( languages, "," );
      if ( language_list.empty() ){
	throw TiCC::OptionError( "invalid languages parameter: " + languages );
      }
    }
    else {
      // so NOT --detectlanguages or --uselanguages
      string language;
      if ( Opts.extract('L', language ) ){
	language = fix_639_1( language );
      }
      if ( !language.empty() ){
	language_list.push_back( language );
      }
    }
  }
  if ( !Opts.empty() ){
    string tomany = Opts.toString();
    throw TiCC::OptionError( "unhandled option(s): " + tomany );
  }
  input_files = Opts.getMassOpts();

  if ( input_files.size() != 0
       && !input_dir.empty() ){
    throw TiCC::OptionError( "found both input files and -I option." );
  }

  if ( batchmode
       && input_files.size() != 0
       && force_xmlin ){
    throw TiCC::OptionError( "found both input files and -F option." );
  }
  if ( batchmode
       && input_files.size() == 0
       && !input_dir.empty() ){
    // get files from inputdir
    if ( force_xmlin ){
      input_files = TiCC::searchFilesExt( input_dir, ".xml", false );
    }
    else {
      input_files = TiCC::searchFiles( input_dir );
    }
    // for ( const auto& bla : input_files ){
    //   cerr << "search: " << bla << endl;
    // }
  }
  if ( !batchmode
       && input_files.size() > 2 ){
    string mess = "found additional arguments on the commandline: "
      + input_files[2] + " ....";
    mess += "\n maybe you want to use batchmode? (option -B)";
    throw TiCC::OptionError( mess );
  }
  file_list = create_file_list();
}

string generate_outname( const string& in, bool force_xmlout ){
  string out;
  auto pos = in.rfind(".xml");
  if ( pos != string::npos ){
    out = in.substr( 0, pos ) + ".ucto.xml";
  }
  else {
    if ( force_xmlout ){
      out = in + ".ucto.xml";
    }
    else {
      pos = in.rfind(".");
      if ( pos != string::npos ){
	out = in.substr( 0, pos ) + ".ucto" + in.substr(pos);
      }
      else {
	out = in + ".ucto";
      }
    }
  }
  return out;
}

vector<pair<string,string>> runtime_opts::create_file_list() {
  // create a list of pairs of input and output filenames
  // adjusted for input and output directories
  vector<pair<string,string>> result;
  if ( !batchmode ){
    if ( input_files.size() > 0 ){
      string in = input_files[0];
      string out;
      if ( input_files.size() == 2 ){
	out = input_files[1];
      }
      pair<string,string> p = create_io_pair( in, out );
      result.push_back( p );
    }
    else {
      result.push_back( make_pair("","") );
    }
  }
  else {
    for ( const auto& in : input_files ){
      string bin = TiCC::basename(in);
      string out = generate_outname( bin, force_xmlout );
      pair<string,string> p = create_io_pair( bin, out );
      result.push_back( p );
    }
  }
  return result;
}

pair<string,string> runtime_opts::create_io_pair( const string& in,
						  const string& out ) {
  pair<string,string> result;
  string in_f = in;
  string out_f = out;
  if ( !input_dir.empty() ){
    in_f = input_dir + in_f;
  }
  if ( !out.empty() ){
    out_f = out;
    if ( !output_dir.empty() ){
      out_f = output_dir + out_f;
    }
  }
  result = make_pair( in_f, out_f );
  return result;
}

pair<istream *,ostream *> runtime_opts::determine_io( const pair<string,string>& file_pair ){
  string in = file_pair.first;
  string out = file_pair.second;
  if ( !in.empty() ){
    ifile = in;
    if ( TiCC::match_back( ifile, ".xml" ) ){
      xmlin = true;
    }
    else {
      xmlin = false;
    }
  }
  if ( !out.empty() ){
    ofile = out;
    if ( TiCC::match_back( ofile, ".xml" ) ){
      xmlout = true;
    }
    else {
      xmlout = force_xmlout;
    }
  }
  check_xmlin_opt();
  check_xmlout_opt();
  if ( !ifile.empty()
       && ifile == ofile ) {
    throw runtime_error( "Output file equals input file! "
			 "Courageously refusing to start..." );
  }
  cerr << "ucto: inputfile = "  << ifile << endl;
  cerr << "ucto: outputfile = " << ofile << endl;
  if ( docid.empty() ) {
    string first_part = ifile.substr( 0, ifile.find(".") );
    if ( folia::isNCName( first_part ) ){ // to be sure
      docid = first_part;
    }
    else {
      docid = "untitleddoc";
    }
  }
  istream *IN = 0;
  if (!xmlin) {
    if ( ifile.empty() ){
      IN = &cin;
    }
    else {
      IN = new ifstream( ifile );
      if ( !IN || !IN->good() ){
	delete IN;
	string mess = "ucto: problems opening inputfile '" + ifile + "'\n"
	  + "ucto: Courageously refusing to start...";
	throw runtime_error( mess );
      }
    }
  }

  ostream *OUT = 0;
  if ( ofile.empty() ){
    OUT = &cout;
  }
  else {
    OUT = new ofstream( ofile );
    if ( !OUT || !OUT->good() ){
      if ( IN != &cin ){
	delete IN;
      }
      string mess  = "ucto: problems opening outputfile '" + ofile + "'\n"
	+ "ucto: Courageously refusing to start...";
      delete OUT;
      throw runtime_error( mess );
    }
  }
  return make_pair(IN,OUT);
}

void init( TokenizerClass& tokenizer,
	   const runtime_opts& my_options ){
  // set debug first, so init() can be debugged too
  tokenizer.setDebug( my_options.debug );
  tokenizer.set_command( my_options.command_line );
  tokenizer.setUttMarker( my_options.utt_marker );
  tokenizer.setVerbose( my_options.verbose );
  tokenizer.setSentenceSplit( my_options.sentencesplit );
  tokenizer.setSentencePerLineOutput( my_options.sentenceperlineoutput );
  tokenizer.setSentencePerLineInput( my_options.sentenceperlineinput );
  tokenizer.setLowercase( my_options.tolowercase );
  tokenizer.setUppercase( my_options.touppercase );
  tokenizer.setNormSet( my_options.norm_set_string );
  tokenizer.setParagraphDetection( my_options.paragraphdetection);
  tokenizer.setQuoteDetection( my_options.quotedetection);
  tokenizer.setNormalization( my_options.normalization );
  tokenizer.setInputEncoding( my_options.inputEncoding );
  tokenizer.setFiltering( my_options.dofiltering );
  tokenizer.setWordCorrection( my_options.docorrectwords );
  tokenizer.setLangDetection( my_options.do_language_detect );
  tokenizer.setPunctFilter( my_options.dopunctfilter );
  tokenizer.setInputClass( my_options.inputclass );
  tokenizer.setOutputClass( my_options.outputclass );
  tokenizer.setCopyClass( my_options.copyclass );
  tokenizer.setXMLOutput( my_options.xmlout, my_options.docid );
  tokenizer.setXMLInput( my_options.xmlin );
  tokenizer.setTextRedundancy( my_options.redundancy );
  tokenizer.setSeparators( my_options.separators ); // IMPORTANT: AFTER setNormalization
  tokenizer.setUndLang( my_options.do_und_lang );
  tokenizer.setNoTags( my_options.ignore_tags );
  tokenizer.setPassThru( my_options.pass_thru );
  if ( !my_options.pass_thru ){
    // init from config file
    if ( !my_options.c_file.empty()
	 && !tokenizer.init( my_options.c_file, my_options.add_tokens ) ){
      throw runtime_error( "ucto: initialize using '" + my_options.c_file
			   + "' failed" );
    }
    else if ( !tokenizer.init( my_options.language_list,
			       my_options.add_tokens ) ){
      throw runtime_error( "ucto: initialize failed" );
    }
    if ( !my_options.c_file.empty() ){
      cerr << "ucto: configured from file: " << my_options.c_file << endl;
    }
    else {
      cerr << "ucto: configured for languages: " << my_options.language_list;
      if ( my_options.do_und_lang ) {
	cerr << ", also the  UND flag is set";
      }
      cerr << endl;
    }
  }
}

int main( int argc, char *argv[] ){
  runtime_opts my_options;
  for ( int i=1; i < argc; ++i ){
    my_options.command_line += " " + string(argv[i]);
  }
  try {
    TiCC::CL_Options Opts( "Bd:e:fhlI:O:PQunmN:vVL:c:s:x:FXT:",
			   "filter:,filterpunct,passthru,textclass:,copyclass,"
			   "inputclass:,outputclass:,normalize:,id:,version,"
			   "help,detectlanguages:,uselanguages:,"
			   "textredundancy:,add-tokens:,split,"
			   "allow-word-corrections,ignore-tag-hints,"
			   "separators:");
    Opts.init(argc, argv );
    if ( Opts.extract( 'h' )
	 || Opts.extract( "help" ) ){
      usage();
      return EXIT_SUCCESS;
    }
    if ( Opts.extract( 'V' ) ||
	 Opts.extract( "version" ) ){
      cout << "Ucto - Unicode Tokenizer - version " << Version() << endl
	   << "(c) CLST 2015 - 2024, Centre for Language and Speech Technology, Radboud University Nijmegen" << endl
	   << "(c) ILK 2009 - 2015, Induction of Linguistic Knowledge Research Group, Tilburg University" << endl
	   << "Licensed under the GNU General Public License v3" << endl;
      cout << "based on [" << folia::VersionName() << "]" << endl;
      return EXIT_SUCCESS;
    }
    my_options.fill( Opts );
  }
  catch( const TiCC::OptionError& e ){
    cerr << "ucto: " << e.what() << endl;
    usage_small();
    return EXIT_FAILURE;
  }
  for ( const auto& io_pair : my_options.file_list ){
    try {
      pair<istream *,ostream *> io_streams;
      io_streams = my_options.determine_io( io_pair );
      istream *IN = io_streams.first;
      ostream *OUT = io_streams.second;
      TokenizerClass tokenizer;
      try {
	init( tokenizer, my_options );
      }
      catch (...){
	if ( IN != &cin ){
	  delete IN;
	}
	if ( OUT != &cout ){
	  delete OUT;
	}
	throw;
      }
      if ( my_options.xmlin ) {
	folia::Document *doc = tokenizer.tokenize_folia( my_options.ifile );
	if ( doc ){
	  *OUT << doc;
	  OUT->flush();
	  delete doc;
	}
	if ( OUT != &cout ){
	  delete OUT;
	}
      }
      else {
	tokenizer.tokenize( *IN, *OUT );
	if ( OUT != &cout ){
	  delete OUT;
	}
	if ( IN != &cin ){
	  delete IN;
	}
      }
    }
    catch ( exception &e ){
      cerr << "ucto: tokenizing '" << io_pair.first << "' to '"
	   << io_pair.second << "' failed: " << e.what() << endl
	   << "continue to next input." << endl;
    }
  }

}
