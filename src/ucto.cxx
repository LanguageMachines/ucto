/*
  Copyright (c) 2006 - 2016
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
#include "ucto/tokenize.h"
#include <unistd.h>

using namespace std;
using namespace Tokenizer;

void usage(){
  cerr << "Usage: " << endl;
  cerr << "\tucto [[options]] [input-file] [[output-file]]"  << endl
       << "Options:" << endl
       << "\t-c <configfile> - Explicitly specify a configuration file" << endl
       << "\t-d <value>      - set debug level" << endl
       << "\t-e <string>     - set input encoding (default UTF8)" << endl
       << "\t-N <string>     - set output normalization (default NFC)" << endl
       << "\t-f              - Disable filtering of special characters" << endl
       << "\t-h or --help    - this message" << endl
       << "\t-L <language>   - Automatically selects a configuration file by language code" << endl
       << "\t-l              - Convert to all lowercase" << endl
       << "\t-u              - Convert to all uppercase" << endl
       << "\t-n              - One sentence per line (output)" << endl
       << "\t-m              - One sentence per line (input)" << endl
       << "\t-v              - Verbose mode" << endl
       << "\t-s <string>     - End-of-Sentence marker (default: <utt>)" << endl
       << "\t--passthru      - Don't tokenize, but perform input decoding and simple token role detection" << endl
       << "\t--normalize=<class1>,class2>,... " << endl
       << "\t                - For class1, class2, etc. output the class tokens instead of the tokens itself." << endl
       << "\t--filterpunct   - remove all punctuation from the output" << endl
       << "\t-P              - Disable paragraph detection" << endl
       << "\t-S              - Disable sentence detection!" << endl
       << "\t-Q              - Enable quote detection (experimental)" << endl
       << "\t-V or --version - Show version information" << endl
       << "\t-x <DocID>      - Output FoLiA XML, use the specified Document ID (obsolete)" << endl
       << "\t-F              - Input file is in FoLiA XML. All untokenised sentences will be tokenised." << endl
       << "\t-X              - Output FoLiA XML, use the Document ID specified with --id=" << endl
       << "\t--id <DocID>    - use the specified Document ID to label the FoLia doc." << endl
       << "\t--textclass <class> - use the specified class to search text in the the FoLia doc. (deprecated. use --inputclass)" << endl
       << "\t--inputclass <class> - use the specified class to search text in the the FoLia doc." << endl
       << "\t--outputclass <class> - use the specified class to output text in the the FoLia doc. (default is 'current'. changing this is dangerous!)" << endl
       << "\t                  (-x and -F disable usage of most other options: -nPQVsS)" << endl;
}

int main( int argc, char *argv[] ){
  int debug = 0;
  bool tolowercase = false;
  bool touppercase = false;
  bool sentenceperlineoutput = false;
  bool sentenceperlineinput = false;
  bool paragraphdetection = true;
  bool quotedetection = false;
  bool dofiltering = true;
  bool dopunctfilter = false;
  bool splitsentences = true;
  bool xmlin = false;
  bool xmlout = false;
  bool verbose = false;
  string eosmarker = "<utt>";
  string docid = "untitleddoc";
  string inputclass = "current";
  string outputclass = "current";
  string normalization = "NFC";
  string inputEncoding = "UTF-8";
  string language = "nld";
  string cfile = "tokconfig-nl";
  string ifile;
  string ofile;
  string c_file;
  string L_file;
  bool passThru = false;
  string norm_set_string;

  try {
    TiCC::CL_Options Opts( "d:e:fhlPQunmN:vVSL:c:s:x:FX",
			   "filterpunct,passthru,textclass:,inputclass:,outputclass:,normalize:,id:,version,help");
    Opts.init(argc, argv );
    if ( Opts.extract( 'h' )
	 || Opts.extract( "help" ) ){
      usage();
      return EXIT_SUCCESS;
    }
    if ( Opts.extract( 'V' ) ||
	 Opts.extract( "version" ) ){
      cout << "Ucto - Unicode Tokenizer - version " << Version() << endl
	   << "(c) ILK 2009 - 2014, Induction of Linguistic Knowledge Research Group, Tilburg University" << endl
	   << "Licensed under the GNU General Public License v3" << endl;
      cout << "based on [" << folia::VersionName() << "]" << endl;
      return EXIT_SUCCESS;
    }
    Opts.extract('e', inputEncoding );
    dofiltering = !Opts.extract( 'f' );
    dopunctfilter = Opts.extract( "filterpunct" );
    paragraphdetection = !Opts.extract( 'P' );
    splitsentences = !Opts.extract( 'S' );
    xmlin = Opts.extract( 'F' );
    quotedetection = Opts.extract( 'Q' );
    Opts.extract( 'c', c_file );
    Opts.extract( 's', eosmarker );
    touppercase = Opts.extract( 'u' );
    tolowercase = Opts.extract( 'l' );
    sentenceperlineoutput = Opts.extract( 'n' );
    sentenceperlineinput = Opts.extract( 'm' );
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
    passThru = Opts.extract( "passthru" );
    Opts.extract( "textclass", inputclass );
    Opts.extract( "inputclass", inputclass );
    Opts.extract( "outputclass", outputclass );
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
    string value;
    if ( Opts.extract('d', value ) ){
      if ( !TiCC::stringTo(value,debug) ){
	throw TiCC::OptionError( "invalid value for -d: " + value );
      }
    }
    if ( Opts.extract('L', language ) ){
      string l = language;
      if ( language ==  "nld" ){
	l = "nl";
	language = "dut";
      }
      else if ( language == "dut" ){
	l = "nl";
      }
      else if ( language ==  "ger" ){
	l = "de";
      }
      else if ( language == "deu" ) {
	l = "de";
	language = "ger";
      }
      else if ( language ==  "fre" ){
	l = "fr";
      }
      else if ( language == "fra" ) {
	l = "fr";
	language = "fre";
      }
      else if ( language == "nl" ){
	language = "dut";
      }
      else if ( language == "en" ){
	language = "eng";
      }
      else if ( language == "de" ){
	language = "ger";
      }
      else if ( language == "fr" ){
	language = "fre";
      }
      L_file = "tokconfig-" + l;
    }
    Opts.extract("normalize", norm_set_string );
    if ( !Opts.empty() ){
      string tomany = Opts.toString();
      throw TiCC::OptionError( "unhandled option(s): " + tomany );
    }
    vector<string> files = Opts.getMassOpts();
    if ( files.size() > 0 ){
      ifile = files[0];
    }
    if ( files.size() > 1 ){
      ofile = files[1];
    }
  }
  catch( const TiCC::OptionError& e ){
    cerr << "ucto: " << e.what() << endl;
    usage();
    exit(EXIT_FAILURE);
  }

  if ( !passThru ){
    if ( !c_file.empty() && !L_file.empty()) {
      cerr << "Error: -L and -c options conflict. Use only one of them." << endl;
      return EXIT_FAILURE;
    }
    if ( !c_file.empty() ){
      cfile = c_file;
    }
    else if ( !L_file.empty() )
      cfile = L_file;
    else {
      cfile = "tokconfig-generic";
    }
  }

  if ((!ifile.empty()) && (ifile == ofile)) {
    cerr << "Error: Output file equals input file! Courageously refusing to start..."  << endl;
    return EXIT_FAILURE;
  }

  if ( !passThru ){
    cerr << "configfile = " << cfile << endl;
  }
  cerr << "inputfile = "  << ifile << endl;
  cerr << "outputfile = " << ofile << endl;

  istream *IN = 0;
  if (!xmlin) {
    if ( ifile.empty() )
      IN = &cin;
    else {
      IN = new ifstream( ifile );
      if ( !IN || !IN->good() ){
	cerr << "Error: problems opening inputfile " << ifile << endl;
	cerr << "Courageously refusing to start..."  << endl;
	return EXIT_FAILURE;
      }
    }
  }

  ostream *OUT = 0;
  if ( ofile.empty() )
    OUT = &cout;
  else {
    OUT = new ofstream( ofile );
  }

  try {
    TokenizerClass tokenizer;
    // set debug first, so init() can be debugged too
    tokenizer.setDebug( debug );
    if ( passThru ){
      tokenizer.setPassThru( true );
    }
    else {
      // init exept for passthru mode
      tokenizer.init( cfile );
    }

    tokenizer.setEosMarker( eosmarker );
    tokenizer.setVerbose( verbose );
    tokenizer.setSentenceDetection( splitsentences ); //detection of sentences
    tokenizer.setSentencePerLineOutput(sentenceperlineoutput);
    tokenizer.setSentencePerLineInput(sentenceperlineinput);
    tokenizer.setLanguage(language);
    tokenizer.setLowercase(tolowercase);
    tokenizer.setUppercase(touppercase);
    tokenizer.setNormSet(norm_set_string);
    tokenizer.setParagraphDetection(paragraphdetection);
    tokenizer.setQuoteDetection(quotedetection);
    tokenizer.setNormalization( normalization );
    tokenizer.setInputEncoding( inputEncoding );
    tokenizer.setFiltering(dofiltering);
    tokenizer.setPunctFilter(dopunctfilter);
    tokenizer.setInputClass(inputclass);
    tokenizer.setOutputClass(outputclass);
    tokenizer.setXMLOutput(xmlout, docid);
    tokenizer.setXMLInput(xmlin);

    if (xmlin) {
      folia::Document doc;
      doc.readFromFile(ifile);
      tokenizer.tokenize(doc);
      *OUT << doc << endl;
    } else {
      tokenizer.tokenize( *IN, *OUT );
      if ( OUT != &cout )
	delete OUT;
      if ( IN != &cin )
	delete IN;
    }
  }
  catch ( exception &e ){
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }

}
