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
       << "\t-I <indir>        - use this intput directory. " << endl
       << "\t-O <outdir>       - set the output directory, results are stored there." << endl
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
       << "\t-X                - Output FoLiA XML, use the Document ID specified with --id=" << endl
       << "\t--id <DocID>      - use the specified Document ID to label the FoLia doc." << endl
       << "                      -X is automatically set when inputfile has extension '.xml'" << endl
       << "\t                  (-x and -F disable usage of most other options: -nPQVs)" << endl
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

class runtime_opts {
public:
  runtime_opts();
  void fill( TiCC::CL_Options& Opts );
  int debug;
  bool tolowercase;
  bool touppercase;
  bool sentenceperlineoutput;
  bool sentenceperlineinput;
  bool paragraphdetection;
  bool quotedetection;
  bool do_language_detect;
  bool dofiltering;
  bool dopunctfilter;
  bool xmlin;
  bool xmlout;
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
  string cfile;
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
};

runtime_opts::runtime_opts():
  debug(0),
  tolowercase(false),
  touppercase(false),
  sentenceperlineoutput(false),
  sentenceperlineinput(false),
  paragraphdetection(true),
  quotedetection(false),
  do_language_detect(false),
  dofiltering(true),
  dopunctfilter(false),
  xmlin(false),
  xmlout(false),
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

void runtime_opts::fill( TiCC::CL_Options& Opts ){
  Opts.extract('e', inputEncoding );
  dopunctfilter = Opts.extract( "filterpunct" );
  docorrectwords = Opts.extract( "allow-word-corrections" );
  paragraphdetection = !Opts.extract( 'P' );
  xmlin = Opts.extract( 'F' );
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
  if ( Opts.extract( 'x', docid ) ){
    cerr << "ucto: The -x option is deprecated and will be removed in a later version.  Please use --id instead" << endl;
    xmlout = true;
    if ( Opts.is_present( 'X' ) ){
      throw TiCC::OptionError( "conflicting options -x and -X" );
    }
    if ( Opts.is_present( "id" ) ){
      throw TiCC::OptionError( "conflicting options -x and --id" );
    }
  }
  else {
    xmlout = Opts.extract( 'X' );
    Opts.extract( "id", docid );
  }
  Opts.extract( 'I', input_dir );
  if ( !input_dir.empty()
       && input_dir.back() != '/' ){
    input_dir += "/";
    if ( !TiCC::isDir(input_dir) ){
      throw TiCC::OptionError( "unable to access '" + input_dir + "'" );
    }
  }
  cerr << "INPUT DIR = " << input_dir << endl;
  Opts.extract( 'O', output_dir );
  if ( !output_dir.empty()
       && output_dir.back() != '/' ){
    output_dir += "/";
    if ( !TiCC::createPath(output_dir) ){
      throw TiCC::OptionError( "unable to write '" + output_dir + "'" );
    }
  }
  cerr << "OUTPUT DIR = " << output_dir << endl;
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
    copyclass = false;
  }
  if ( Opts.extract( 'f' ) ){
    cerr << "ucto: The -f option is deprecated and will be removed in a later version.  Please use --filter=NO instead" << endl;
    dofiltering = false;
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
  if ( sentencesplit ){
    if ( xmlout ){
      throw TiCC::OptionError( "conflicting options --split and -x or -X" );
    }
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
  if ( Opts.extract('d', value ) ){
    if ( !TiCC::stringTo(value,debug) ){
      throw TiCC::OptionError( "invalid value for -d: " + value );
    }
  }
  ignore_tags = Opts.extract( "ignore-tag-hints" );
  pass_thru = Opts.extract( "passthru" );
  Opts.extract( "separators", separators );
  bool use_lang = Opts.is_present( "uselanguages" );
  bool detect_lang = Opts.is_present( "detectlanguages" );
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
	throw TiCC::OptionError( "invalid language list: " + languages );
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
  Opts.extract("normalize", norm_set_string );
  if ( !Opts.empty() ){
    string tomany = Opts.toString();
    throw TiCC::OptionError( "unhandled option(s): " + tomany );
  }
  vector<string> files = Opts.getMassOpts();
  if ( files.size() > 0 ){
    ifile = files[0];
    if ( !input_dir.empty() ){
      ifile = input_dir + ifile;
    }
    if ( TiCC::match_back( ifile, ".xml" ) ){
      xmlin = true;
    }
  }
  if ( use_lang && !xmlin ){
    throw TiCC::OptionError( "--uselanguages is only valid for FoLiA input" );
  }
  if ( docorrectwords && !xmlin ){
    throw TiCC::OptionError( "--allow-word-corrections is only valid for FoLiA input" );
  }
  if ( files.size() == 2 ){
    ofile = files[1];
    if ( TiCC::match_back( ofile, ".xml" ) ){
      xmlout = true;
    }
    if ( !output_dir.empty() ){
      ofile = output_dir + ofile;
    }
  }
  if ( files.size() > 2 ){
    string mess = "found additional arguments on the commandline: "
      + files[2] + "....";
    throw TiCC::OptionError( mess );
  }
}

int main( int argc, char *argv[] ){
  int debug = 0;
  bool do_language_detect = false;
  bool do_und_lang = false;
  string redundancy = "minimal";
  string utt_marker = "<utt>";
  string docid;// = "untitleddoc";
  vector<string> language_list;
  string cfile;
  string ifile;
  string ofile;
  string input_dir;
  string output_dir;
  string c_file;
  bool pass_thru = false;
  string add_tokens;
  string command_line = "ucto";
  for ( int i=1; i < argc; ++i ){
    command_line += " " + string(argv[i]);
  }
  runtime_opts my_options;
  try {
    TiCC::CL_Options Opts( "d:e:fhlI:O:PQunmN:vVL:c:s:x:FXT:",
			   "filter:,filterpunct,passthru,textclass:,copyclass,"
			   "inputclass:,outputclass:,normalize:,id:,version,"
			   "help,detectlanguages:,uselanguages:,"
			   "textredundancy:,add-tokens:,split,"
			   "allow-word-corrections,ignore-tag-hints,"
			   "separators:");
    TiCC::CL_Options opts_copy( "d:e:fhlI:O:PQunmN:vVL:c:s:x:FXT:",
				"filter:,filterpunct,passthru,textclass:,copyclass,"
				"inputclass:,outputclass:,normalize:,id:,version,"
				"help,detectlanguages:,uselanguages:,"
				"textredundancy:,add-tokens:,split,"
				"allow-word-corrections,ignore-tag-hints,"
				"separators:");
    opts_copy.init(argc, argv );
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
    my_options.fill( opts_copy );
    Opts.extract( 's', utt_marker );
    Opts.extract( 'T', redundancy );
    if ( Opts.extract( 'x', docid ) ){
      cerr << "ucto: The -x option is deprecated and will be removed in a later version.  Please use --id instead" << endl;
      if ( Opts.is_present( 'X' ) ){
	throw TiCC::OptionError( "conflicting options -x and -X" );
      }
      if ( Opts.is_present( "id" ) ){
	throw TiCC::OptionError( "conflicting options -x and --id" );
      }
    }
    else {
      Opts.extract( "id", docid );
    }
    Opts.extract( 'I', input_dir );
    if ( !input_dir.empty()
	 && input_dir.back() != '/' ){
      input_dir += "/";
      if ( !TiCC::isDir(input_dir) ){
	throw TiCC::OptionError( "unable to access '" + input_dir + "'" );
      }
    }
    cerr << "INPUT DIR = " << input_dir << endl;
    Opts.extract( 'O', output_dir );
    if ( !output_dir.empty()
	 && output_dir.back() != '/' ){
      output_dir += "/";
      if ( !TiCC::createPath(output_dir) ){
	throw TiCC::OptionError( "unable to write '" + output_dir + "'" );
      }
    }
    cerr << "OUTPUT DIR = " << output_dir << endl;
    Opts.extract( "add-tokens", add_tokens );
    string value;
    if ( Opts.extract('d', value ) ){
      if ( !TiCC::stringTo(value,debug) ){
	throw TiCC::OptionError( "invalid value for -d: " + value );
      }
    }
    pass_thru = Opts.extract( "passthru" );
    bool use_lang = Opts.is_present( "uselanguages" );
    bool detect_lang = Opts.is_present( "detectlanguages" );
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
	  throw TiCC::OptionError( "invalid language list: " + languages );
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
    vector<string> files = Opts.getMassOpts();
    if ( files.size() > 0 ){
      ifile = files[0];
      if ( !input_dir.empty() ){
	ifile = input_dir + ifile;
      }
    }
    if ( files.size() == 2 ){
      ofile = files[1];
      if ( !output_dir.empty() ){
	ofile = output_dir + ofile;
      }
    }
    if ( files.size() > 2 ){
      cerr << "found additional arguments on the commandline: " << files[2]
	   << "...." << endl;
      return EXIT_FAILURE;
    }
  }
  catch( const TiCC::OptionError& e ){
    cerr << "ucto: " << e.what() << endl;
    usage();
    return EXIT_FAILURE;
  }
  if ( !pass_thru ){
    set<string> available_languages = Setting::installed_languages();
    if ( !c_file.empty() ){
      cfile = c_file;
    }
    else if ( available_languages.empty() ){
      cerr << "ucto: The uctodata package seems not to be installed." << endl;
      cerr << "ucto: Installing uctodata is a prerequisite." << endl;
      return EXIT_FAILURE;
    }
    else if ( language_list.empty()
	      && !do_language_detect ){
      cerr << "ucto: missing a language specification (-L or --detectlanguages or --uselanguages option)" << endl;
      cerr << "ucto: Available Languages: ";
      for( const auto& l : available_languages ){
	cerr << l << ",";
      }
      cerr << endl;
      return EXIT_FAILURE;
    }
    else {
      auto it = std::find( language_list.begin(),
			   language_list.end(),
			   "und" );
      if ( it != language_list.end() ){
	language_list.erase( it );
	do_und_lang = true;
      }
      for ( const auto& l : language_list ){
	if ( available_languages.find(l) == available_languages.end() ){
	  cerr << "ucto: unsupported language '" << l << "'" << endl;
	  cerr << "ucto: Available Languages: ";
	  for( const auto& lang : available_languages ){
	    cerr << lang << ",";
	  }
	  cerr << endl;
	  return EXIT_FAILURE;
	}
      }
    }
  }
  if ((!ifile.empty()) && (ifile == ofile)) {
    cerr << "ucto: Output file equals input file! Courageously refusing to start..."  << endl;
    return EXIT_FAILURE;
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
  if (!my_options.xmlin) {
    if ( ifile.empty() ){
      IN = &cin;
    }
    else {
      IN = new ifstream( ifile );
      if ( !IN || !IN->good() ){
	cerr << "ucto: problems opening inputfile " << ifile << endl;
	cerr << "ucto: Courageously refusing to start..."  << endl;
	delete IN;
	return EXIT_FAILURE;
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
      cerr << "ucto: problems opening outputfile " << ofile << endl;
      cerr << "ucto: Courageously refusing to start..."  << endl;
      delete OUT;
      if ( IN != &cin ){
	delete IN;
      }
      return EXIT_FAILURE;
    }
  }
  try {
    TokenizerClass tokenizer;
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
    tokenizer.setXMLOutput( my_options.xmlout, docid );
    tokenizer.setXMLInput( my_options.xmlin );
    tokenizer.setTextRedundancy( my_options.redundancy );
    tokenizer.setSeparators( my_options.separators ); // IMPORTANT: AFTER setNormalization
    tokenizer.setUndLang( my_options.do_und_lang );
    if ( my_options.ignore_tags ){
      tokenizer.setNoTags( true );
    }
    if ( my_options.pass_thru ){
      tokenizer.setPassThru( true );
    }
    else {
      // init from config file
      if ( !cfile.empty()
	   && !tokenizer.init( cfile, add_tokens ) ){
	if ( IN != &cin ){
	  delete IN;
	}
	if ( OUT != &cout ){
	  delete OUT;
	}
	return EXIT_FAILURE;
      }
      else if ( !tokenizer.init( my_options.language_list, add_tokens ) ){
	if ( IN != &cin ){
	  delete IN;
	}
	if ( OUT != &cout ){
	  delete OUT;
	}
	return EXIT_FAILURE;
      }
      if ( !cfile.empty() ){
	cerr << "ucto: configured from file: " << cfile << endl;
      }
      else {
	cerr << "ucto: configured for languages: " << my_options.language_list;
	if ( do_und_lang ) {
	  cerr << ", also the  UND flag is set";
	}
	cerr << endl;
      }
    }
    if ( my_options.xmlin) {
      folia::Document *doc = tokenizer.tokenize_folia( ifile );
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
    cerr << "ucto: " << e.what() << endl;
    return EXIT_FAILURE;
  }

}
