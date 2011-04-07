/*
  $Id$
  $URL$
  Copyright (c) 1998 - 2011
  ILK  -  Tilburg University
  CNTS -  University of Antwerp
 
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
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      Timbl@uvt.nl
*/
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <string>
#include <iostream>
#include <fstream>
#include "ucto/tokenize.h"
#include <unistd.h>
#include "config.h"

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
       << "\t-L <language>   - Automatically selects a configuration file by language code" << endl
       << "\t-l              - Convert to all lowercase" << endl
       << "\t-u              - Convert to all uppercase" << endl       
       << "\t-n              - One sentence per line (output)" << endl
       << "\t-m              - One sentence per line (input)" << endl
       << "\t-V              - Verbose mode" << endl
       << "\t-s <string>     - End-of-Sentence marker (default: <utt>)" << endl
       << "\t-P              - Disable paragraph detection" << endl
       << "\t-S              - Disable sentence detection!" << endl
       << "\t-Q              - Disable quote detection" << endl
       << "\t-v              - Show version information" << endl
       << "\t-x <DocID>      - Output FoLiA XML, use the specified Document ID" << endl
       << "\t                  (-x disables usage of most other options: -nulPQVsS)" << endl;
}

int main( int argc, char *argv[] ){
  int debug = 0;
  bool tolowercase = false;
  bool touppercase = false;
  bool sentenceperlineoutput = false;
  bool sentenceperlineinput = false;
  bool paragraphdetection = true;
  bool quotedetection = true;
  bool dofiltering = true;
  bool splitsentences = true;
  bool xmlout = false;
  bool verbose = false;
  string eosmarker = "<utt>";
  string docid = "untitleddoc";
  string normalization = "NFC";
  string inputEncoding = "UTF-8";
  string cdir = string(SYSCONF_PATH) + "/ucto/";
  string cfile = "tokconfig-en";
  string ifile;
  string ofile;
  string c_file;
  string L_file;

  static struct option longOpts[] = { { "passthru", 0, 0, 1 },
				      { 0,0,0,0} };

  int opt;
  int longOpt;
  bool passThru = false;
  try {
    while ((opt = getopt_long( argc, argv, 
			       "d:e:fhlPQunmN:vVSL:c:s:x:",
			       longOpts, &longOpt )) != -1) {
      switch (opt)
	{  
	case 1: passThru = true; break;
	case 'd': debug = stringTo<int>(optarg); break;
	case 'e': inputEncoding = optarg; break;
	case 'f': dofiltering = false; break;
	case 'P': paragraphdetection = false; break;
	case 'Q': quotedetection = false; break;
	case 'c': c_file = optarg; break;
	case 's': eosmarker = optarg; break;
	case 'S': splitsentences = false; break;
	case 'L': L_file = string("tokconfig-") + string(optarg); break;
	case 'u': touppercase = true; break;
	case 'l': tolowercase = true; break;
	case 'n': sentenceperlineoutput = true; break;
    case 'm': sentenceperlineinput = true; break;
	case 'N': normalization = optarg; break;
	case 'V': verbose = true; break;
	case 'v': cout << "Ucto - Unicode Tokenizer - version " << VERSION << endl << "(c) ILK 2009 - 2011, Induction of Linguistic Knowledge Research Group, Tilburg University" << endl << "Licensed under the GNU General Public License v3" << endl; return EXIT_SUCCESS;
	case 'x': xmlout = true; docid = optarg; break;
	default: usage(); return EXIT_SUCCESS;
	}
    }
  }
  catch ( exception &e ){
    cerr << "Error in option '" << char(opt) << "' : " << e.what() << endl << endl;
    usage();
    return EXIT_FAILURE;
  }

  if ( !passThru ){
    if ( !c_file.empty() && !L_file.empty()) {
      cerr << "Error: -L and -c options conflict. Use only one of them." << endl;
      return EXIT_FAILURE;
    }
    if ( !c_file.empty() )
      cfile = c_file;
    else if ( !L_file.empty() )
      cfile = L_file;
    else { 
      cerr << "Error: Please specify either a language (-L) or a configuration file (-c)" << endl;
      usage();
      return EXIT_FAILURE;
    }
  }
  
  if (optind < argc) ifile = argv[optind++];
  if (optind < argc) ofile = argv[optind++];
  
  if ((!ifile.empty()) && (ifile == ofile)) {
    cerr << "Error: Output file equals input file! Courageously refusing to start..."  << endl;
    return EXIT_FAILURE;
  }

  if ( !passThru ){
    cerr << "configdir = " << cdir << endl;
    cerr << "configfile = " << cfile << endl;
  }
  cerr << "inputfile = "  << ifile << endl;
  cerr << "outputfile = " << ofile << endl;

  istream *IN = 0;
  if ( ifile.empty() )
    IN = &cin;
  else {
    IN = new ifstream( ifile.c_str() );
    if ( !IN || !IN->good() ){
      cerr << "Error: problems opening inputfile " << ifile << endl;
      cerr << "Courageously refusing to start..."  << endl;
      return EXIT_FAILURE;
    }
  }
  ostream *OUT = 0;
  if ( ofile.empty() )
    OUT = &cout;
  else {
    OUT = new ofstream( ofile.c_str() );
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
      tokenizer.init( cdir, cfile );
    }

    tokenizer.setEosMarker( eosmarker );
    tokenizer.setVerbose( verbose );
    tokenizer.setSentenceDetection( splitsentences ); //detection of sentences
    tokenizer.setSentencePerLineOutput(sentenceperlineoutput);
    tokenizer.setSentencePerLineInput(sentenceperlineinput);
    tokenizer.setLowercase(tolowercase);
    tokenizer.setUppercase(touppercase);
    tokenizer.setParagraphDetection(paragraphdetection);
    tokenizer.setQuoteDetection(quotedetection);
    tokenizer.setNormalization( normalization );
    tokenizer.setInputEncoding( inputEncoding );
    tokenizer.setFiltering(dofiltering);
    tokenizer.setXMLOutput(xmlout, docid);

    tokenizer.tokenize(IN,OUT);
    if ( OUT != &cout )
      delete OUT;
    if ( IN != &cin )
      delete IN;
  }
  catch ( exception &e ){
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }
  
}
