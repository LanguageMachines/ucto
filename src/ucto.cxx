/*
  Copyright (c) 2006 - 2021
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
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/PrettyPrint.h"
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
       << "\t-d <value>        - set debug level" << endl
       << "\t-e <string>       - set input encoding (default UTF8)" << endl
       << "\t-N <string>       - set output normalization (default NFC)" << endl
       << "\t--filter=[YES|NO] - Disable filtering of special characters" << endl
       << "\t-f                - OBSOLETE. use --filter=NO" << endl
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
       << "\t\t DISABLED!, Produces invalid FoLiA. Needs a fix" << endl
       << "\t--ignore-tag-hints - Do NOT use tag=\"token\" hints from the FoLiA input. (default is to use them)" << endl
       << "\t--filterpunct      - remove all punctuation from the output" << endl
       << "\t--uselanguages=<lang1,lang2,..langn> - Using FoLiA input, only tokenize strings in these languages. Default = 'lang1'" << endl
       << "\t--detectlanguages=<lang1,lang2,..langn> - try to assign a language to each line of text input. Default = 'lang1'" << endl
       << "\t--add-tokens='file' - add additional tokens to the [TOKENS] of the" << endl
       << "\t                    default language. TOKENS are always kept intact." << endl
       << "\t-P                - Disable paragraph detection" << endl
       << "\t-Q                - Enable quote detection (experimental)" << endl
       << "\t-V or --version   - Show version information" << endl
       << "\t-x <DocID>        - Output FoLiA XML, use the specified Document ID (obsolete)" << endl
       << "\t-F                - Input file is in FoLiA XML. All untokenized sentences will be tokenized." << endl
       << "\t                    -F is automatically set when inputfile has extension '.xml'" << endl
       << "\t-X                - Output FoLiA XML, use the Document ID specified with --id=" << endl
       << "\t--id <DocID>      - use the specified Document ID to label the FoLia doc." << endl
       << "                      -X is automatically set when inputfile has extension '.xml'" << endl
       << "\t--inputclass <class>  - use the specified class to search text in the FoLia doc.(default is 'current')" << endl
       << "\t--outputclass <class> - use the specified class to output text in the FoLia doc. (default is 'current')" << endl
       << "\t--textclass <class>   - use the specified class for both input and output of text in the FoLia doc. (default is 'current'). Implies --filter=NO." << endl
       << "\t                  (-x and -F disable usage of most other options: -nPQVs)" << endl;
}

int main( int argc, char *argv[] ){
  int debug = 0;
  bool tolowercase = false;
  bool touppercase = false;
  bool sentenceperlineoutput = false;
  bool sentenceperlineinput = false;
  bool paragraphdetection = true;
  bool quotedetection = false;
  bool do_language_detect = false;
  bool dofiltering = true;
  bool dopunctfilter = false;
  bool xmlin = false;
  bool xmlout = false;
  bool verbose = false;
  bool docorrectwords = false;
  string redundancy = "minimal";
  string eosmarker = "<utt>";
  string docid = "untitleddoc";
  string normalization = "NFC";
  string inputEncoding = "UTF-8";
  string inputclass  = "current";
  string outputclass = "current";
  vector<string> language_list;
  string cfile;
  string ifile;
  string ofile;
  string c_file;
  bool pass_thru = false;
  bool ignore_tags = false;
  bool sentencesplit = false;
  string norm_set_string;
  string add_tokens;
  string command_line = "ucto";
  for ( int i=1; i < argc; ++i ){
    command_line += " " + string(argv[i]);
  }
  try {
    TiCC::CL_Options Opts( "d:e:fhlPQunmN:vVL:c:s:x:FXT:",
			   "filter:,filterpunct,passthru,textclass:,inputclass:,outputclass:,normalize:,id:,version,help,detectlanguages:,uselanguages:,textredundancy:,add-tokens:,split,allow-word-corrections,ignore-tag-hints");
    Opts.init(argc, argv );
    if ( Opts.extract( 'h' )
	 || Opts.extract( "help" ) ){
      usage();
      return EXIT_SUCCESS;
    }
    if ( Opts.extract( 'V' ) ||
	 Opts.extract( "version" ) ){
      cout << "Ucto - Unicode Tokenizer - version " << Version() << endl
	   << "(c) CLST 2015 - 2021, Centre for Language and Speech Technology, Radboud University Nijmegen" << endl
	   << "(c) ILK 2009 - 2015, Induction of Linguistic Knowledge Research Group, Tilburg University" << endl
	   << "Licensed under the GNU General Public License v3" << endl;
      cout << "based on [" << folia::VersionName() << "]" << endl;
      return EXIT_SUCCESS;
    }
    Opts.extract('e', inputEncoding );
    dopunctfilter = Opts.extract( "filterpunct" );
    docorrectwords = Opts.extract( "allow-word-corrections" );
    if ( docorrectwords ){
      cerr << "option --allow-word-corrections DISABLED!. Produces invalid FoLiA!" << endl;
      return EXIT_FAILURE;
    }
    paragraphdetection = !Opts.extract( 'P' );
    xmlin = Opts.extract( 'F' );
    quotedetection = Opts.extract( 'Q' );
    Opts.extract( 's', eosmarker );
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
    if ( sentencesplit ){
      if ( xmlout ){
	throw TiCC::OptionError( "conflicting options --split and -x or -X" );
      }
      //      sentenceperlineoutput = true;
    }
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
    if ( Opts.extract( 'f' ) ){
      cerr << "ucto: The -f option is used.  Please consider using --filter=NO" << endl;
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
	if ( TiCC::split_at( languages, language_list, "," ) < 1 ){
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
    else if ( language_list.empty() ){
      cerr << "ucto: missing a language specification (-L or --detectlanguages or --uselanguages option)" << endl;
      if ( available_languages.size() == 1
	   && *available_languages.begin() == "generic" ){
	cerr << "ucto: The uctodata package seems not to be installed." << endl;
	cerr << "ucto: You can use '-L generic' to run a simple default tokenizer."
	     << endl;
	cerr << "ucto: Installing uctodata is highly recommended." << endl;
      }
      else {
	cerr << "ucto: Available Languages: ";
	for( const auto& l : available_languages ){
	  cerr << l << ",";
	}
	cerr << endl;
      }
      return EXIT_FAILURE;
    }
    else {
      for ( const auto& l : language_list ){
	if ( available_languages.find(l) == available_languages.end() ){
	  cerr << "ucto: unsupported language '" << l << "'" << endl;
	  if ( available_languages.size() == 1
	       && *available_languages.begin() == "generic" ){
	    cerr << "ucto: The uctodata package seems not to be installed." << endl;
	    cerr << "ucto: You can use '-L generic' to run a simple default tokenizer."
		 << endl;
	    cerr << "ucto: Installing uctodata is highly recommended." << endl;
	  }
	  else {
	    cerr << "ucto: Available Languages: ";
	    for( const auto& lang : available_languages ){
	      cerr << lang << ",";
	    }
	    cerr << endl;
	  }
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

  istream *IN = 0;
  if (!xmlin) {
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
    tokenizer.setDebug( debug );
    tokenizer.set_command( command_line );
    tokenizer.setEosMarker( eosmarker );
    tokenizer.setVerbose( verbose );
    tokenizer.setSentenceSplit(sentencesplit);
    tokenizer.setSentencePerLineOutput(sentenceperlineoutput);
    tokenizer.setSentencePerLineInput(sentenceperlineinput);
    tokenizer.setLowercase(tolowercase);
    tokenizer.setUppercase(touppercase);
    tokenizer.setNormSet(norm_set_string);
    tokenizer.setParagraphDetection(paragraphdetection);
    tokenizer.setQuoteDetection(quotedetection);
    tokenizer.setNormalization( normalization );
    tokenizer.setInputEncoding( inputEncoding );
    tokenizer.setFiltering(dofiltering);
    tokenizer.setWordCorrection(docorrectwords);
    tokenizer.setLangDetection(do_language_detect);
    tokenizer.setPunctFilter(dopunctfilter);
    tokenizer.setInputClass(inputclass);
    tokenizer.setOutputClass(outputclass);
    tokenizer.setXMLOutput(xmlout, docid);
    tokenizer.setXMLInput(xmlin);
    tokenizer.setTextRedundancy(redundancy);
    if ( ignore_tags ){
      tokenizer.setNoTags( true );
    }
    if ( pass_thru ){
      tokenizer.setPassThru( true );
    }
    else {
      // init exept for passthru mode
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
      else if ( !tokenizer.init( language_list, add_tokens ) ){
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
	cerr << "ucto: configured for languages: " << language_list << endl;
      }
    }


    if (xmlin) {
      folia::Document *doc = tokenizer.tokenize_folia( ifile );
      if ( doc ){
	*OUT << doc;
	OUT->flush();
	delete doc;
      }
    }
    else {
      tokenizer.tokenize( *IN, *OUT );
      if ( OUT != &cout )
	delete OUT;
      if ( IN != &cin )
	delete IN;
    }
  }
  catch ( exception &e ){
    cerr << "ucto: " << e.what() << endl;
    return EXIT_FAILURE;
  }

}
