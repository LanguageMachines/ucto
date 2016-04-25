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

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include "config.h"
#include "unicode/ustream.h"
#include "unicode/regex.h"
#include "unicode/ucnv.h"
#include "unicode/schriter.h"
#include "ucto/unicode.h"
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"

using namespace std;
using namespace TiCC;

namespace Tokenizer {

  std::string Version() { return VERSION; }
  std::string VersionName() { return PACKAGE_STRING; }
  string defaultConfigDir = string(SYSCONF_PATH) + "/ucto/";

  enum ConfigMode { NONE, RULES, ABBREVIATIONS, ATTACHEDPREFIXES, ATTACHEDSUFFIXES, PREFIXES, SUFFIXES, TOKENS, UNITS, ORDINALS, EOSMARKERS, QUOTES, FILTER, RULEORDER, METARULES };

  class uRangeError: public std::out_of_range {
  public:
    uRangeError( const string& s ): out_of_range( "ucto: out of range :" + s ){};
  };

  class uLogicError: public std::logic_error {
  public:
    uLogicError( const string& s ): logic_error( "ucto: logic error :" + s ){};
  };

  class uOptionError: public std::invalid_argument {
  public:
    uOptionError( const string& s ): invalid_argument( "ucto: option :" + s ){};
  };

  class uConfigError: public std::invalid_argument {
  public:
    uConfigError( const string& s ): invalid_argument( "ucto: config file :" + s ){};
    uConfigError( const UnicodeString& us ): invalid_argument( "ucto: config file :" + folia::UnicodeToUTF8(us) ){};
  };

  class uCodingError: public std::runtime_error {
  public:
    uCodingError( const string& s ): runtime_error( "ucto: coding problem :" + s ){};
  };


  class UnicodeRegexMatcher {
  public:
    UnicodeRegexMatcher( const UnicodeString& );
    ~UnicodeRegexMatcher();
    bool match_all( const UnicodeString&, UnicodeString&, UnicodeString&  );
    const UnicodeString get_match( unsigned int ) const;
    int NumOfMatches() const;
    int split( const UnicodeString&, vector<UnicodeString>& );
    UnicodeString Pattern() const{ return pattern->pattern(); }
  private:
    UnicodeRegexMatcher( const UnicodeRegexMatcher& );  // inhibit copies
    UnicodeRegexMatcher& operator=( const UnicodeRegexMatcher& ); // inhibit copies
    string failString;
    RegexPattern *pattern;
    RegexMatcher *matcher;
    UnicodeRegexMatcher();
    vector<UnicodeString> results;
  };

  UnicodeRegexMatcher::UnicodeRegexMatcher( const UnicodeString& pat ){
    failString = "";
    matcher = NULL;
    UErrorCode u_stat = U_ZERO_ERROR;
    UParseError errorInfo;
    pattern = RegexPattern::compile( pat, 0, errorInfo, u_stat );
    if ( U_FAILURE(u_stat) ){
      failString = "Invalid regular expression '" + folia::UnicodeToUTF8(pat)
	+ "' ";
      if ( errorInfo.offset >0 )
	failString += ", at position " + toString( errorInfo.offset );
      throw uConfigError(failString);
    }
    else {
      matcher = pattern->matcher( u_stat );
      if (U_FAILURE(u_stat)){
	failString = "unable to create PatterMatcher with pattern '" +
	  folia::UnicodeToUTF8(pat) + "'";
	throw uConfigError(failString);
      }
    }
  }

  UnicodeRegexMatcher::~UnicodeRegexMatcher(){
    delete pattern;
    delete matcher;
  }

  //#define MATCH_DEBUG 1

  bool UnicodeRegexMatcher::match_all( const UnicodeString& line,
				       UnicodeString& pre,
				       UnicodeString& post ){
    UErrorCode u_stat = U_ZERO_ERROR;
    pre = "";
    post = "";
    results.clear();
    if ( matcher ){
#ifdef MATCH_DEBUG
      cerr << "start matcher [" << line << "], pattern = " << Pattern() << endl;
#endif
      matcher->reset( line );
      if ( matcher->find() ){
#ifdef MATCH_DEBUG
	cerr << "matched " << folia::UnicodeToUTF8(line) << endl;
	for ( int i=0; i <= matcher->groupCount(); ++i ){
	  cerr << "group[" << i << "] =" << matcher->group(i,u_stat) << endl;
	}
#endif
	if ( matcher->groupCount() == 0 ){
	  // case 1: a rule without capture groups matches
	  UnicodeString us = matcher->group(0,u_stat) ;
#ifdef MATCH_DEBUG
	  cerr << "case 1, result = " << us << endl;
#endif
	  results.push_back( us );
	  int start = matcher->start( 0, u_stat );
	  if ( start > 0 ){
	    pre = UnicodeString( line, 0, start );
#ifdef MATCH_DEBUG
	    cerr << "found pre " << folia::UnicodeToUTF8(pre) << endl;
#endif
	  }
	  int end = matcher->end( 0, u_stat );
	  if ( end < line.length() ){
	    post = UnicodeString( line, end );
#ifdef MATCH_DEBUG
	    cerr << "found post " << folia::UnicodeToUTF8(post) << endl;
#endif
	  }
	  return true;
	}
	else if ( matcher->groupCount() == 1 ){
	  // case 2: a rule with one capture group matches
	  int start = matcher->start( 1, u_stat );
	  if ( start >= 0 ){
	    UnicodeString us = matcher->group(1,u_stat) ;
#ifdef MATCH_DEBUG
	    cerr << "case 2a , result = " << us << endl;
#endif
	    results.push_back( us );
	    if ( start > 0 ){
	      pre = UnicodeString( line, 0, start );
#ifdef MATCH_DEBUG
	      cerr << "found pre " << pre << endl;
#endif
	    }
	    int end = matcher->end( 1, u_stat );
	    if ( end < line.length() ){
	      post = UnicodeString( line, end );
#ifdef MATCH_DEBUG
	      cerr << "found post " << post << endl;
#endif
	    }
	  }
	  else {
	    // group 1 is empty, return group 0
	    UnicodeString us = matcher->group(0,u_stat) ;
#ifdef MATCH_DEBUG
	    cerr << "case 2b , result = " << us << endl;
#endif
	    results.push_back( us );
	    start = matcher->start( 0, u_stat );
	    if ( start > 0 ){
	      pre = UnicodeString( line, 0, start );
#ifdef MATCH_DEBUG
	      cerr << "found pre " << pre << endl;
#endif
	    }
	    int end = matcher->end( 0, u_stat );
	    if ( end < line.length() ){
	      post = UnicodeString( line, end );
#ifdef MATCH_DEBUG
	      cerr << "found post " << post << endl;
#endif
	    }
	  }
	  return true;
	}
	else {
	  // a rule with more then 1 capture group
	  // this is quite ugly...
	  int end = 0;
	  for ( int i=0; i <= matcher->groupCount(); ++i ){
#ifdef MATCH_DEBUG
	    cerr << "group " << i << endl;
#endif
	    u_stat = U_ZERO_ERROR;
	    int start = matcher->start( i, u_stat );
#ifdef MATCH_DEBUG
	    cerr << "start = " << start << endl;
#endif
	    if (!U_FAILURE(u_stat)){
	      if ( start < 0 ){
		continue;
	      }
	    }
	    else
	      break;
	    if ( start > end ){
	      pre = UnicodeString( line, end, start );
#ifdef MATCH_DEBUG
	      cerr << "found pre " << folia::UnicodeToUTF8(pre) << endl;
#endif
	    }
	    end = matcher->end( i, u_stat );
#ifdef MATCH_DEBUG
	    cerr << "end = " << end << endl;
#endif
	    if (!U_FAILURE(u_stat)){
	      results.push_back( UnicodeString( line, start, end - start ) );
#ifdef MATCH_DEBUG
	      cerr << "added result " << folia::UnicodeToUTF8( results[results.size()-1] ) << endl;
#endif
	    }
	    else
	      break;
	  }
	  if ( end < line.length() ){
	    post = UnicodeString( line, end );
#ifdef MATCH_DEBUG
	    cerr << "found post " << folia::UnicodeToUTF8(post) << endl;
#endif
	  }
	  return true;
	}
      }
    }
    results.clear();
    return false;
  }

  const UnicodeString UnicodeRegexMatcher::get_match( unsigned int n ) const{
    if ( n < results.size() )
      return results[n];
    else
      return "";
  }

  int UnicodeRegexMatcher::NumOfMatches() const {
    if ( results.size() > 0 )
      return results.size()-1;
    else
      return 0;
  }

  int UnicodeRegexMatcher::split( const UnicodeString& us,
				  vector<UnicodeString>& result ){
    result.clear();
    const int maxWords = 256;
    UnicodeString words[maxWords];
    UErrorCode status = U_ZERO_ERROR;
    int numWords = matcher->split( us, words, maxWords, status );
    for ( int i = 0; i < numWords; ++i )
      result.push_back( words[i] );
    return numWords;
  }

  const UnicodeString type_currency = "CURRENCY";
  const UnicodeString type_emoticon = "EMOTICON";
  const UnicodeString type_word = "WORD";
  const UnicodeString type_punctuation = "PUNCTUATION";
  const UnicodeString type_number = "NUMBER";
  const UnicodeString type_unknown = "UNKNOWN";

  ostream& operator<<( ostream& os, const Quoting& q ){
    for( size_t i=0; i < q.quotes.size(); ++i ){
      os << q.quotes[i].openQuote << "\t" << q.quotes[i].closeQuote << endl;
    }
    return os;
  }

  void Quoting::flushStack( int beginindex ) {
    //flush up to (but not including) the specified index
    if ( !quotestack.empty() ){
      std::vector<int> new_quoteindexstack;
      std::vector<UChar32> new_quotestack;
      for ( size_t i = 0; i < quotestack.size(); i++) {
	if (quoteindexstack[i] >= beginindex ) {
	  new_quotestack.push_back(quotestack[i]);
	  new_quoteindexstack.push_back(quoteindexstack[i]-beginindex);
	}
      }
      quoteindexstack = new_quoteindexstack;
      quotestack = new_quotestack;
    }
  }

  void Quoting::add( const UnicodeString& o, const UnicodeString& c ){
    QuotePair quote;
    quote.openQuote = o;
    quote.closeQuote = c;
    quotes.push_back( quote );
  }

  int Quoting::lookup( const UnicodeString& open, int& stackindex ){
    if (quotestack.empty() || (quotestack.size() != quoteindexstack.size())) return -1;
    auto it = quotestack.crbegin();
    size_t i = quotestack.size();
    while ( it != quotestack.crend() ){
      if ( open.indexOf( *it ) >= 0 ){
 	stackindex = i-1;
 	return quoteindexstack[stackindex];
      }
      --i;
      ++it;
    }
    return -1;
  }

  UnicodeString Quoting::lookupOpen( const UnicodeString &q ) const {
    for ( size_t i=0; i < quotes.size(); ++i ){
      if ( quotes[i].openQuote.indexOf(q) >=0 )
	return quotes[i].closeQuote;
    }
    return "";
  }

  UnicodeString Quoting::lookupClose( const UnicodeString &q ) const {
    UnicodeString res;
    for ( size_t i=0; i < quotes.size(); ++i ){
      if ( quotes[i].closeQuote.indexOf(q) >= 0 )
	return quotes[i].openQuote;
    }
    return "";
  }

  Token::Token( const UnicodeString& _type,
		const UnicodeString& _s,
		TokenRole _role): type(_type), us(_s), role(_role) {}


  std::string Token::texttostring() { return folia::UnicodeToUTF8(us); }
  std::string Token::typetostring() { return folia::UnicodeToUTF8(type); }

  ostream& operator<< (std::ostream& os, const Token& t ){
    os << t.type << " : " << t.role  << ":" << t.us;
    return os;
  }

  Rule::~Rule() {
    delete regexp;
  }

  Rule::Rule( const UnicodeString& _id, const UnicodeString& _pattern):
    id(_id), pattern(_pattern) {
    regexp = new UnicodeRegexMatcher(pattern);
  }

  ostream& operator<< (std::ostream& os, const Rule& r ){
    if ( r.regexp ){
      os << r.id << "=\"" << r.regexp->Pattern() << "\"";
    }
    else
      os << r.id  << "=NULL";
    return os;
  }

  TokenizerClass::TokenizerClass():
    linenum(0),
    inputEncoding( "UTF-8" ), eosmark("<utt>"),
    tokDebug(0), verbose(false),
    detectBounds(true), detectQuotes(false),
    doFilter(true), doPunctFilter(false),
    detectPar(true),
    paragraphsignal(true),
    sentenceperlineoutput(false), sentenceperlineinput(false),
    lowercase(false), uppercase(false),
    xmlout(false), passthru(false),
    inputclass("current"), outputclass("current")
  {
    theErrLog = new TiCC::LogStream(cerr);
    theErrLog->setstamp( NoStamp );

    //TODO: add test ICU for bug http://bugs.icu-project.org/trac/ticket/8826, if it's
    //fixed assume 8824 (?i) handing -  is fixed too:
    //test if pattern (?i)aßx matched against aßx produces results, if not, bug
    //is present and users should be warned

  }

  void TokenizerClass::setErrorLog( TiCC::LogStream *os ) {
    if ( theErrLog != os ){
      delete theErrLog;
    }
    theErrLog = os;
  }

  string TokenizerClass::setInputEncoding( const std::string& enc ){
    string old = inputEncoding;
    inputEncoding = enc;
    return old;
  }

  void stripCR( string& s ){
    string::size_type pos = s.rfind( '\r' );
    if ( pos != string::npos ){
      s.erase( pos );
    }
  }

  vector<Token> TokenizerClass::tokenizeStream( istream& IN, bool allatonce) {
    vector<Token> outputTokens;
    bool done = false;
    bool bos = true;
    string line;
    do {
      done = !getline( IN, line );
      linenum++;
      if ( tokDebug > 0 ){
	*Log(theErrLog) << "[tokenize] Read input line " << linenum << endl;
      }
      stripCR( line );
      if ( sentenceperlineinput )
	line += string(" ") + folia::UnicodeToUTF8(eosmark);
      int numS;
      if ( (done) || (line.empty()) ){
	signalParagraph();
	numS = countSentences(true); //count full sentences in token buffer, force buffer to empty!
      } else {
	if ( passthru )
	  passthruLine( line, bos );
	else
	  tokenizeLine( line );
  	  numS = countSentences(); //count full sentences in token buffer
      }
      if ( numS > 0 ) { //process sentences
	if ( tokDebug > 0 ){
	  *Log(theErrLog) << "[tokenize] " << numS << " sentence(s) in buffer, processing..." << endl;
	}
	for (int i = 0; i < numS; i++) {
	  vector<Token> v = getSentence( i );
	  outputTokens.insert( outputTokens.end(), v.begin(), v.end() );
	}
	// clear processed sentences from buffer
	if ( tokDebug > 0 ){
	  *Log(theErrLog) << "[tokenize] flushing " << numS << " sentence(s) from buffer..." << endl;
	}
	flushSentences(numS);
	if (!allatonce) return outputTokens;
      }
      else {
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[tokenize] No sentences yet, reading on..." << endl;
	}
      }
    } while (!done);
    return outputTokens;
  }

  string TokenizerClass::tokenizeSentenceStream( istream& IN ) {
    string result;
    int numS = countSentences(); //count full sentences in token buffer
    if ( numS > 0 ) { // still some sentences in the buffer
      if  (tokDebug > 0) {
	*Log(theErrLog) << "[tokenizeStream] " << numS
			<< " sentence(s) in buffer, processing..." << endl;
      }
      result = getSentenceString( 0 );
      // clear processed sentence from buffer
      if  (tokDebug > 0){
	*Log(theErrLog) << "[tokenizeStream] flushing 1 "
			<< " sentence from buffer..." << endl;
      }
      flushSentences(1);
      return result;
    }
    bool done = false;
    bool bos = true;
    string line;
    do {
      done = !getline( IN, line );
      linenum++;
      if (tokDebug > 0) {
	*Log(theErrLog) << "[tokenize] Read input line " << linenum << endl;
      }
      stripCR( line );
      if ( sentenceperlineinput )
	line += string(" ") + folia::UnicodeToUTF8(eosmark);
      if ( (done) || (line.empty()) ){
	signalParagraph();
	numS = countSentences(true); //count full sentences in token buffer, force buffer to empty!
      }
      else {
	if ( passthru )
	  passthruLine( line, bos );
	else
	  tokenizeLine( line );
	numS = countSentences(); //count full sentences in token buffer
      }
      if ( numS > 0 ) {
	// 1 or more sentences in the buffer.
	// extract the first 1
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[tokenizeStream] " << numS << " sentence(s) in buffer, processing first one..." << endl;
	}
	result = getSentenceString( 0 );
	//clear processed sentence from buffer
	if  (tokDebug > 0){
	  *Log(theErrLog) << "[tokenizeStream] flushing 1 "
			  << " sentence(s) from buffer..." << endl;
	}
	flushSentences(1);
	return result;
      }
      else {
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[tokenizeStream] No sentence yet, reading on..." << endl;
	}
      }
    } while (!done);
    return result;
  }

  folia::Document *TokenizerClass::tokenize( istream& IN ) {
    folia::Document *doc = new folia::Document( "id='" + docid + "'" );
    outputTokensDoc_init( *doc );
    folia::FoliaElement *root = doc->doc()->index(0);
    int parCount = 0;
    vector<Token> buffer;
    do {
	vector<Token> v = tokenizeStream( IN , true);
	for ( auto iter = v.cbegin(); iter != v.cend(); ++iter ) {
	    if (iter->role & NEWPARAGRAPH) {
		//process the buffer
		parCount= outputTokensXML( root, buffer, parCount);
		buffer.clear();
	    }
	    buffer.push_back( *iter);
	}
    } while (!IN.eof());
    if (!buffer.empty()) outputTokensXML( root, buffer, parCount);
    return doc;
  }

  void TokenizerClass::tokenize( const string & ifile, const string & ofile) {
    ostream *OUT = NULL;
    if ( ofile.empty() )
        OUT = &cout;
    else {
        OUT = new ofstream( ofile );
    }

    istream *IN = NULL;
    if (!xmlin) {
        if ( ifile.empty() )
            IN = &cin;
        else {
            IN = new ifstream( ifile );
            if ( !IN || !IN->good() ){
                cerr << "Error: problems opening inputfile " << ifile << endl;
                cerr << "Courageously refusing to start..."  << endl;
                exit(EXIT_FAILURE);
            }
        }
      this->tokenize( *IN, *OUT );
    } else {
      folia::Document doc;
      doc.readFromFile(ifile);
      this->tokenize(doc);
      *OUT << doc << endl;
    }

    if ( IN != &cin ) delete IN;
    if ( OUT != &cout ) delete OUT;
  }

  void TokenizerClass::tokenize( istream& IN, ostream& OUT) {
    if (xmlout) {
      folia::Document *doc = tokenize( IN );
      OUT << doc << endl;
      delete doc;
    } else {
      int i = 0;
      do {
       vector<Token> v = tokenizeStream( IN , false);
       if (!v.empty()) outputTokens( OUT, v , (i>0) );
       i++;
      } while (!IN.eof());
      OUT << endl;
    }
  }

  bool TokenizerClass::tokenize( folia::Document& doc ) {
    if ( tokDebug >= 2 ){
      *Log(theErrLog) << "tokenize doc " << doc << endl;
    }
    for ( size_t i = 0; i < doc.doc()->size(); i++) {
      if (tokDebug >= 2) {
	*Log(theErrLog) << "[tokenize] Invoking processing of first-level element " << doc.doc()->index(i)->id() << endl;
      }
      tokenizeElement( doc.doc()->index(i) );
    }
    return true;
  }

  void appendText( folia::FoliaElement *root,
		   const string& outputclass  ){
    //    cerr << endl << "appendText:" << root->id() << endl;
    if ( root->hastext( outputclass ) ){
      return;
    }
    UnicodeString utxt = root->text( outputclass, false, false );
    // cerr << "untok: '" << utxt << "'" << endl;
    // UnicodeString txt = root->text( outputclass, true );
    // cerr << "  tok: '" << txt << "'" << endl;
    root->settext( folia::UnicodeToUTF8(utxt), outputclass );
  }


  void TokenizerClass::tokenizeElement(folia::FoliaElement * element) {
    if ( element->isinstance(folia::Word_t)
	 || element->isinstance(folia::TextContent_t))
      // shortcut
      return;
    if ( tokDebug >= 2 ){
      *Log(theErrLog) << "[tokenizeElement] Processing FoLiA element " << element->id() << endl;
    }
    if ( element->hastext( inputclass ) ) {
      // We have an element which contains text. That's nice
      // now we must see wether some 'formatting' is there. ( like Words() or
      // Sentences() )
      // If so: assume that the text is tokenized already, and don't spoil that
      if ( element->isinstance(folia::Paragraph_t) ) {
	//tokenize paragraph: check for absence of sentences
	vector<folia::Sentence*> sentences = element->sentences();
	if (sentences.size() > 0) {
	  // bail out
	  return;
	}
      }
      else if ( ( element->isinstance(folia::Sentence_t) )
		|| ( element->isinstance(folia::Head_t) ) ) {
	//tokenize sentence: check for absence of Word's
	vector<folia::Word*> words = element->words();
	if (words.size() > 0) {
	  // bail out
	  return;
	}
      }
      else {
	// Some other element that contains text. Probably deeper.
	// look it up. skip all paragraphs and sentences
	vector<folia::Paragraph*> paragraphs = element->paragraphs();
	if (paragraphs.size() > 0) {
	  // already paragraphs, bail out
	  return;
	}
	vector<folia::Sentence*> sentences = element->sentences();
	if (sentences.size() > 0) {
	  // already sentences, bail out
	  return;
	}
	vector<folia::Word*> words = element->words();
	if (words.size() > 0) {
	  // already words, bail out
	  return;
	}
      }
      if ( inputclass != outputclass && outputclass == "current" ){
	if ( element->hastext( outputclass ) ){
	  throw uLogicError( "cannot set text with class='current' on node "
			     + element->id() +
			     " because it already has text in that class." );
	}
      }
      // so we have text, in an element without 'formatting' yet, good
      // lets Tokenize the available text!
      tokenizeSentenceElement( element );
      return;
    }
    //recursion step for textless elements
    if ( tokDebug >= 2 ){
      *Log(theErrLog) << "[tokenizeElement] Processing children of FoLiA element " << element->id() << endl;
    }
    for ( size_t i = 0; i < element->size(); i++) {
      tokenizeElement( element->index(i));
    }
    return;
  }

  void TokenizerClass::tokenizeSentenceElement( folia::FoliaElement *element ){
    folia::Document *doc = element->doc();
    if ( passthru ){
      doc->declare( folia::AnnotationType::TOKEN, "passthru", "annotator='ucto', annotatortype='auto', datetime='now()'" );
    }
    else {
      doc->declare( folia::AnnotationType::TOKEN, settingsfilename, "annotator='ucto', annotatortype='auto', datetime='now()'" );
    }
    if  ( tokDebug > 0 ){
      cerr << "tokenize sentence element: " << element->id() << endl;
    }
    UnicodeString line = element->stricttext( inputclass );
    if ( line.isEmpty() ){
      // so no usefull text in this element. skip it
      return;
    }
    line += " "  + eosmark;
    if (tokDebug >= 1){
      *Log(theErrLog) << "[tokenizeSentenceElement] Processing sentence:"
		      << line << endl;
    }
    if ( passthru ){
      bool bos = true;
      passthruLine( folia::UnicodeToUTF8(line), bos );
    }
    else
      tokenizeLine( line );
    //ignore EOL data, we have by definition only one sentence:
    int numS = countSentences(true); //force buffer to empty
    vector<Token> outputTokens;
    for (int i = 0; i < numS; i++) {
      vector<Token> v = getSentence( i );
      outputTokens.insert( outputTokens.end(), v.begin(), v.end() );
    }
    outputTokensXML( element, outputTokens );
    flushSentences(numS);
  }

  void TokenizerClass::outputTokensDoc_init( folia::Document& doc ) const {
    doc.addStyle( "text/xsl", "folia.xsl" );
    if ( passthru ){
      doc.declare( folia::AnnotationType::TOKEN, "passthru", "annotator='ucto', annotatortype='auto', datetime='now()'" );
    }
    else {
      doc.declare( folia::AnnotationType::TOKEN, settingsfilename,
		   "annotator='ucto', annotatortype='auto', datetime='now()'");
    }
    folia::Text *text = new folia::Text( folia::getArgs("id='" + docid + ".text'") );
    doc.append( text );
  }

  void TokenizerClass::outputTokensDoc( folia::Document& doc,
					const vector<Token>& tv ) const {
    folia::FoliaElement *root = doc.doc()->index(0);
    outputTokensXML(root, tv );
  }

  int TokenizerClass::outputTokensXML( folia::FoliaElement *root, const vector<Token>& tv, int parCount ) const {
    short quotelevel = 0;
    folia::FoliaElement *lastS = 0;
    if  (tokDebug > 0) {
      *Log(theErrLog) << "[outputTokensXML] root=<" << root->classname()
		      << ">" << endl;
      *Log(theErrLog) << "[outputTokensXML] root-id=" << root->id() << endl;
    }
    bool root_is_sentence = false;
    bool root_is_structure_element = false;
    if ( root->isinstance( folia::Sentence_t ) ){
      lastS = root;
      root_is_sentence = true;
    }
    else if ( root->isinstance( folia::Paragraph_t )
	      || root->isinstance( folia::Head_t )
	      || root->isinstance( folia::Note_t )
	      || root->isinstance( folia::ListItem_t )
	      || root->isinstance( folia::Part_t )
	      || root->isinstance( folia::Caption_t )
	      || root->isinstance( folia::Event_t ) ){
      root_is_structure_element = true;
    }

    bool in_paragraph = false;
    for ( size_t i=0; i < tv.size(); i++) {
      if ( ( !root_is_structure_element && !root_is_sentence ) &&
	   ( (tv[i].role & NEWPARAGRAPH) || !in_paragraph ) ) {
	if ( in_paragraph ){
	  appendText( root, outputclass );
	  root = root->parent();
	}
	if ( tokDebug > 0 ) {
	  *Log(theErrLog) << "[outputTokensXML] Creating paragraph" << endl;
	}
	folia::KWargs args;
	args["id"] = root->doc()->id() + ".p." +  toString(++parCount);
	folia::FoliaElement *p = new folia::Paragraph( args, root->doc() );
	//	*Log(theErrLog) << "created " << p << endl;
	root->append( p );
	root = p;
	quotelevel = 0;
      }
      if ( tv[i].role & ENDQUOTE) {
	if ( tokDebug > 0 ){
	  *Log(theErrLog) << "[outputTokensXML] End of quote" << endl;
	}
	quotelevel--;
	root = root->parent();
	lastS = root;
	if ( tokDebug > 0 ){
	  *Log(theErrLog) << "[outputTokensXML] back to " << root->classname() << endl;
	}
      }
      if (( tv[i].role & BEGINOFSENTENCE) && (!root_is_sentence)) {
	folia::KWargs args;
	if ( root->id().empty() )
	  args["generate_id"] = root->parent()->id();
	else
	  args["generate_id"] = root->id();
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[outputTokensXML] Creating sentence in '"
			  << args["generate_id"] << "'" << endl;
	}
	folia::FoliaElement *s = new folia::Sentence( args, root->doc() );
	// *Log(theErrLog) << "created " << s << endl;
	root->append( s );
	root = s;
	lastS = root;
      }
      if  (tokDebug > 0) {
	*Log(theErrLog) << "[outputTokensXML] Creating word element for " << tv[i].us << endl;
      }
      folia::KWargs args;
      args["generate_id"] = lastS->id();
      args["class"] = folia::UnicodeToUTF8( tv[i].type );
      if ( passthru )
	args["set"] = "passthru";
      else
	args["set"] = settingsfilename;
      if ( tv[i].role & NOSPACE) {
	args["space"]= "no";
      }
      folia::FoliaElement *w = new folia::Word( args, root->doc() );
      UnicodeString out = tv[i].us;
      if (lowercase) {
	out.toLower();
      }
      else if (uppercase) {
	out.toUpper();
      }
      w->settext( folia::UnicodeToUTF8( out ), outputclass );
      //      *Log(theErrLog) << "created " << w << " text= " <<  tv[i].us << endl;
      root->append( w );
      if ( tv[i].role & BEGINQUOTE) {
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[outputTokensXML] Creating quote element" << endl;
	}
	folia::FoliaElement *q = new folia::Quote( folia::getArgs( "generate_id='" + root->id() + "'"),
						    root->doc() );
	//	*Log(theErrLog) << "created " << q << endl;
	root->append( q );
	root = q;
	quotelevel++;
      }
      if ( ( tv[i].role & ENDOFSENTENCE) && (!root_is_sentence) ) {
	if  (tokDebug > 0) {
	  *Log(theErrLog) << "[outputTokensXML] End of sentence" << endl;
	}
	appendText( root, outputclass );
	root = root->parent();
	lastS = root;
	if  (tokDebug > 0){
	  *Log(theErrLog) << "[outputTokensXML] back to " << root->classname() << endl;
	}
      }
      in_paragraph = true;
    }
    if ( tv.size() > 0 ){
      appendText( root, outputclass );
    }
    return parCount;
  }

  ostream& operator<<( ostream& os, const TokenRole& tok ){
    if ( tok & NOSPACE) os << "NOSPACE ";
    if ( tok & BEGINOFSENTENCE) os << "BEGINOFSENTENCE ";
    if ( tok & ENDOFSENTENCE) os << "ENDOFSENTENCE ";
    if ( tok & NEWPARAGRAPH) os << "NEWPARAGRAPH ";
    if ( tok & BEGINQUOTE) os << "BEGINQUOTE ";
    if ( tok & ENDQUOTE) os << "ENDQUOTE ";
    return os;
  }

  void TokenizerClass::outputTokens( ostream& OUT, const vector<Token>& toks, const bool continued) const {
    //continued should be set to true when outputTokens is invoked multiple times and it is not the first invokation, makes paragraph boundaries work over multiple calls
    short quotelevel = 0;
    for ( size_t i = 0; i < toks.size(); i++) {
      if ((detectPar) && ( toks[i].role & NEWPARAGRAPH) && (!verbose) && ((i != 0) || (continued))  ) {
	//output paragraph separator
	if (sentenceperlineoutput) {
	  OUT << endl;
	} else {
	  OUT << endl << endl;
	}
      }
      if (lowercase) {
	UnicodeString s = toks[i].us;
	OUT << s.toLower();
      }
      else if (uppercase) {
	UnicodeString s = toks[i].us;
	OUT << s.toUpper();
      }
      else {
	OUT << toks[i].us;
      }
      if ( toks[i].role & NEWPARAGRAPH) quotelevel = 0;
      if ( toks[i].role & BEGINQUOTE) quotelevel++;
      if (verbose) {
	OUT << "\t" << toks[i].type << "\t" << toks[i].role << endl;
      }
      if ( toks[i].role & ENDQUOTE) quotelevel--;

      if ( toks[i].role & ENDOFSENTENCE) {
	if (verbose) {
	  OUT << endl;
	}
	else if (quotelevel == 0) {
	  if (sentenceperlineoutput) {
	    OUT << endl;
	  }
	  else {
	    OUT << " " + eosmark;
	  }
	}
      }
      if ( (i < toks.size() ) && (!verbose) ) {
	if (!( ( toks[i].role & ENDOFSENTENCE) && (sentenceperlineoutput) )) {
	  OUT << " ";
	  //FBK: ADD SPACE WITHIN QUOTE CONTEXT IN ANY CASE
	}
	else if ((quotelevel > 0) && (sentenceperlineoutput)) {
	  OUT << " ";
	}
      }
    }
  }

  int TokenizerClass::countSentences(bool forceentirebuffer) {
    //Return the number of *completed* sentences in the token buffer

    //Performs  extra sanity checks at the same time! Making sure
    //BEGINOFSENTENCE and ENDOFSENTENCE always pair up, and that TEMPENDOFSENTENCE roles
    //are converted to proper ENDOFSENTENCE markers

    short quotelevel = 0;
    int count = 0;
    const int size = tokens.size();
    int begin = 0;
    for (int i = begin; i < size; i++) {
      if (tokDebug >= 5){
	*Log(theErrLog) << "[countSentences] buffer#" <<i
			<< " word=[" << tokens[i].us
			<< "] role=" << tokens[i].role
			<< ", quotelevel="<< quotelevel << endl;
      }
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;
      if ((forceentirebuffer) && (tokens[i].role & TEMPENDOFSENTENCE) && (quotelevel == 0)) {
	//we thought we were in a quote, but we're not... No end quote was found and an end is forced now.
	//Change TEMPENDOFSENTENCE to ENDOFSENTENCE and make sure sentences match up sanely
	tokens[i].role ^= TEMPENDOFSENTENCE;
	tokens[i].role |= ENDOFSENTENCE;
	tokens[begin].role |= BEGINOFSENTENCE;
      }
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	begin = i + 1;
	count++;
	if (tokDebug >= 5){
	  *Log(theErrLog) << "[countSentences] SENTENCE #" << count << " found" << endl;
	}
	if ((begin < size) ){
	  tokens[begin].role |= BEGINOFSENTENCE;
	}
      }
      if ((forceentirebuffer) && (i == size - 1) && !(tokens[i].role & ENDOFSENTENCE))  {
	//last token of buffer
	count++;
	tokens[i].role |= ENDOFSENTENCE;
	if (tokDebug >= 5){
	  *Log(theErrLog) << "[countSentences] SENTENCE #" << count << " *FORCIBLY* ended" << endl;
	}
      }
    }
    return count;
  }

  int TokenizerClass::flushSentences( int sentences ) {
    //Flush n sentences from the buffer, returns the number of tokens left
    short quotelevel = 0;
    const int size = tokens.size();
    if (sentences == 0) return size;
    int begin = 0;
    for (int i = 0; (i < size ) && (sentences > 0); i++) {
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	begin = i + 1;
	--sentences;
      }
    }
    if (begin == 0) {
      throw uLogicError("Unable to flush, not so many sentences in buffer");
    }
    if (begin == size) {
      tokens.clear();
      quotes.clearStack();
    }
    else {
      tokens.erase (tokens.begin(),tokens.begin()+begin);
      if (!quotes.emptyStack()) {
	quotes.flushStack( begin );
      }
    }
    //After flushing, the first token still in buffer (if any) is always a BEGINOFSENTENCE:
    if (!tokens.empty()) {
      tokens[0].role |= BEGINOFSENTENCE;
    }
    return tokens.size();
  }

  vector<Token> TokenizerClass::getSentence( int index ) {
    vector<Token> outToks;
    int count = 0;
    const int size = tokens.size();
    short quotelevel = 0;
    size_t begin = 0;
    size_t end = 0;
    for ( int i = 0; i < size; i++) {
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & ENDQUOTE) quotelevel--;
      if ((tokens[i].role & BEGINOFSENTENCE) && (quotelevel == 0)) {
	begin = i;
      }
      //FBK: QUOTELEVEL GOES UP BEFORE begin IS UPDATED... RESULTS IN DUPLICATE OUTPUT
      if (tokens[i].role & BEGINQUOTE) quotelevel++;

      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	if (count == index) {
	  end = i;
	  tokens[begin].role |= BEGINOFSENTENCE;  //sanity check
	  if (tokDebug >= 1){
	    *Log(theErrLog) << "[tokenize] extracted sentence " << index << ", begin="<<begin << ",end="<< end << endl;
	  }
	  for ( size_t i=begin; i <= end; ++i ){
	    outToks.push_back( tokens[i] );
	  }
	  return outToks;
	}
	count++;
      }
    }
    throw uRangeError( "No sentence exists with the specified index: "
		       + toString( index ) );
    return outToks;
  }

  string TokenizerClass::getSentenceString( unsigned int i ){
    vector<Token> v = getSentence( i );
    if ( !v.empty() ){
      //This only makes sense in non-verbose mode, force verbose=false
      stringstream TMPOUT;
      const bool tv = verbose;
      verbose = false;
      outputTokens( TMPOUT, v );
      verbose = tv;
      return TMPOUT.str();
    }
    return "";
  }

  vector<string> TokenizerClass::getSentences() {
    vector<string> sentences;
    int numS = countSentences(true); //force buffer to empty
    for (int i = 0; i < numS; i++) {
      string tmp = getSentenceString( i );
      sentences.push_back( tmp );
    }
    return sentences;
  }

  // FBK: return true if character is a quote.
  bool TokenizerClass::u_isquote( UChar32 c ) const {
    bool quote = false;
    if ( u_hasBinaryProperty( c, UCHAR_QUOTATION_MARK ) ||
	 c == '`' ) {
      // the backtick is often used as a quote (apostroph)
      // but is DOESN`T have the UCHAR_QUOTATION_MARK property
      quote = true;
    }
    else {
      UnicodeString opening = quotes.lookupOpen( c );
      if (!opening.isEmpty()) {
	quote = true;
      }
      else {
	UnicodeString closing = quotes.lookupClose( c );
	if (!closing.isEmpty()) {
	  quote = true;
	}
      }
    }
    return quote;
  }

  //FBK: USED TO CHECK IF CHARACTER AFTER QUOTE IS AN BOS.
  //MOSTLY THE SAME AS ABOVE, EXCEPT WITHOUT CHECK FOR PUNCTUATION
  //BECAUSE: '"Hoera!", zei de man' MUST NOT BE SPLIT ON ','..
  bool is_BOS( UChar32 c ){
    bool is_bos = false;
    UBlockCode s = ublock_getCode(c);
    //test for languages that distinguish case
    if ( (s == UBLOCK_BASIC_LATIN) || (s == UBLOCK_GREEK)
	 || (s == UBLOCK_CYRILLIC) || (s == UBLOCK_GEORGIAN)
	 || (s == UBLOCK_ARMENIAN) || (s == UBLOCK_DESERET)) {
      if ( u_isupper(c) || u_istitle(c) ) {
	//next 'word' starts with more punctuation or with uppercase
	is_bos = true;
      }
    }
    return is_bos;
  }

  bool TokenizerClass::resolveQuote(int endindex, const UnicodeString& open ) {
    //resolve a quote
    int stackindex = -1;
    int beginindex = quotes.lookup( open, stackindex );

    if (beginindex >= 0) {
      if (tokDebug >= 2) {
	*Log(theErrLog) << "[resolveQuote] Quote found, begin="<< beginindex << ", end="<< endindex << endl;
      }

      if (beginindex > endindex) {
	throw uRangeError( "Begin index for quote is higher than end index!" );
      }

      //We have a quote!

      //resolve sentences within quote, all sentences must be full sentences:
      int beginsentence = beginindex + 1;
      int expectingend = 0;
      int subquote = 0;
      int size = tokens.size();
      for (int i = beginsentence; i < endindex; i++) {
	if (tokens[i].role & BEGINQUOTE) subquote++;

	if (subquote == 0) {
	  if (tokens[i].role & BEGINOFSENTENCE) expectingend++;
	  if (tokens[i].role & ENDOFSENTENCE) expectingend--;

	  if (tokens[i].role & TEMPENDOFSENTENCE) {
	    tokens[i].role ^= TEMPENDOFSENTENCE;
	    tokens[i].role |= ENDOFSENTENCE;
	    tokens[beginsentence].role |= BEGINOFSENTENCE;
	    beginsentence = i + 1;
	  }
	  // In case of nested quoted sentences, such as:
	  //    MvD: "Nou, Van het Gouden Been ofzo herinner ik mij als kind: 'Waar is mijn gouden been?'"
	  // the BEGINOFSENTENCE is only set for the inner quoted sentence 'Waar is mijn gouden been'. However,
	  // We also need one for the outser sentence.
	}
	else if ( (tokens[i].role & ENDQUOTE)
		  && (tokens[i].role & ENDOFSENTENCE)) {
	  tokens[beginsentence].role |= BEGINOFSENTENCE;
	  beginsentence = i + 1;
	}
	if (tokens[i].role & ENDQUOTE) subquote--;
      }
      if ((expectingend == 0) && (subquote == 0)) {
	//ok, all good, mark the quote:
	tokens[beginindex].role |= BEGINQUOTE;
	tokens[endindex].role |= ENDQUOTE;
      }
      else if ((expectingend == 1) && (subquote == 0) && !(tokens[endindex - 1].role & ENDOFSENTENCE)) {
	//missing one endofsentence, we can correct, last token in quote token is endofsentence:
	if ( tokDebug >= 2 ) {
	  *Log(theErrLog) << "[resolveQuote] Missing endofsentence in quote, fixing... " << expectingend << endl;
	}
	tokens[endindex - 1].role |= ENDOFSENTENCE;
	//mark the quote
	tokens[beginindex].role |= BEGINQUOTE;
	tokens[endindex].role |= ENDQUOTE;
      }
      else {
	if ( tokDebug >= 2) {
	  *Log(theErrLog) << "[resolveQuote] Quote can not be resolved, unbalanced sentences or subquotes within quote, skipping... (expectingend=" << expectingend << ",subquote=" << subquote << ")" << endl;
	}
	//something is wrong. Sentences within quote are not balanced, so we won't mark the quote.
      }
      //remove from stack (ok, granted, stack is a bit of a misnomer here)
      quotes.eraseAtPos( stackindex );
      //FBK: ENDQUOTES NEED TO BE MARKED AS ENDOFSENTENCE IF THE PREVIOUS TOKEN
      //WAS AN ENDOFSENTENCE. OTHERWISE THE SENTENCES WILL NOT BE SPLIT.
      if ((tokens[endindex].role & ENDQUOTE) && (tokens[endindex-1].role & ENDOFSENTENCE)) {
        //FBK: CHECK FOR EOS AFTER QUOTES
        if ((endindex+1 == size) || //FBK: endindex EQUALS TOKEN SIZE, MUST BE EOSMARKERS
            ((endindex + 1 < size) && (is_BOS(tokens[endindex+1].us[0])))) {
	  tokens[endindex].role |= ENDOFSENTENCE;
	  // FBK: CHECK IF NEXT TOKEN IS A QUOTE AND NEXT TO THE QUOTE A BOS
        }
	else if ( (endindex + 2 < size)
		  && (u_isquote(tokens[endindex+1].us[0]))
		  && (is_BOS(tokens[endindex+2].us[0]))) {
	  tokens[endindex].role |= ENDOFSENTENCE;
	  // If the current token is an ENDQUOTE and the next token is a quote and also the last token,
	  // the current token is an EOS.
        }
	else if ( (endindex + 2 == size)
		  && (u_isquote(tokens[endindex+1].us[0]))) {
	  tokens[endindex].role |= ENDOFSENTENCE;
        }
      }
      return true;
    }
    else {
      return false;
    }
  }

  bool TokenizerClass::detectEos( size_t i ) const {
    bool is_eos = false;
    UChar32 c = tokens[i].us.char32At(0);
    if ( c == '.' || eosmarkers.indexOf( c ) >= 0 ){
      if (i + 1 == tokens.size() ) {	//No next character?
	is_eos = true; //Newline after eosmarker
      }
      else {
	UChar32 c = tokens[i+1].us.char32At(0);
	if ( u_isquote(c) ){
	  // next word is quote
	  if ( detectQuotes )
	    is_eos = true;
	  else if ( i + 2 < tokens.size() ) {
	    UChar32 c = tokens[i+2].us.char32At(0);
	    if ( u_isupper(c) || u_istitle(c) || u_ispunct(c) ){
	      //next 'word' after quote starts with uppercase or is punct
	      is_eos = true;
	    }
	  }
	}
	else if ( tokens[i].us.length() > 1 ){
	  // PUNCTUATION multi...
	  if ( u_isupper(c) || u_istitle(c) )
	    is_eos = true;
	}
	else
	  is_eos = true;
      }
    }
    return is_eos;
  }

  void TokenizerClass::detectQuoteBounds( const int i ) {
    UChar32 c = tokens[i].us.char32At(0);
    //Detect Quotation marks
    if ((c == '"') || ( UnicodeString(c) == "＂") ) {
      if (tokDebug > 1 ){
	*Log(theErrLog) << "[detectQuoteBounds] Standard double-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c)) {
	if (tokDebug > 1 ) {
	  *Log(theErrLog) << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else if ( c == '\'' ) {
      if (tokDebug > 1 ){
	*Log(theErrLog) << "[detectQuoteBounds] Standard single-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c)) {
	if (tokDebug > 1 ) {
	  *Log(theErrLog) << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else {
      UnicodeString close = quotes.lookupOpen( c );
      if ( !close.isEmpty() ){ // we have a opening quote
	if ( tokDebug > 1 ) {
	  *Log(theErrLog) << "[detectQuoteBounds] Opening quote found @i="<< i << ", pushing to stack for resultion later..." << endl;
	}
	quotes.push( i, c ); // remember it
      }
      else {
	UnicodeString open = quotes.lookupClose( c );
	if ( !open.isEmpty() ) { // we have a closeing quote
	  if (tokDebug > 1 ) {
	    *Log(theErrLog) << "[detectQuoteBounds] Closing quote found @i="<< i << ", attempting to resolve..." << endl;
	  }
	  if (!resolveQuote(i, open )) { // resolve the matching opening
	    if (tokDebug > 1 ) {
	      *Log(theErrLog) << "[detectQuoteBounds] Unable to resolve" << endl;
	    }
	  }
	}
      }
    }
  }

  bool isClosing( const Token& tok ){
    if ( tok.us.length() == 1 &&
	 ( tok.us[0] == ')' || tok.us[0] == '}'
	   || tok.us[0] == ']' || tok.us[0] == '>' ) )
      return true;
    return false;
  }

  void TokenizerClass::detectSentenceBounds( const int offset ){
    //find sentences
    const int size = tokens.size();
    for (int i = offset; i < size; i++) {
      if (tokDebug > 1 ){
	*Log(theErrLog) << "[detectSentenceBounds] i="<< i << " word=["
			<< tokens[i].us
			<< "] type=" << tokens[i].type
			<< ", role=" << tokens[i].role << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ) {
	// we have some kind of punctuation. Does it mark an eos?
	bool is_eos = detectEos( i );
	if (is_eos) {
	  if ((tokDebug > 1 )){
	    *Log(theErrLog) << "[detectSentenceBounds] EOS FOUND @i="
			    << i << endl;
	  }
	  tokens[i].role |= ENDOFSENTENCE;
	  //if this is the end of the sentence, the next token is the beginning of a new one
	  if ((i + 1 < size) && !(tokens[i+1].role & BEGINOFSENTENCE))
	    tokens[i+1].role |= BEGINOFSENTENCE;
	  //if previous token is EOS and not BOS, it will stop being EOS, as this one will take its place
	  if ((i > 0) && (tokens[i-1].role & ENDOFSENTENCE) && !(tokens[i-1].role & BEGINOFSENTENCE) ) {
	    tokens[i-1].role ^= ENDOFSENTENCE;
	    if (tokens[i].role & BEGINOFSENTENCE) {
	      tokens[i].role ^= BEGINOFSENTENCE;
	    }
	  }
	}
	else if ( isClosing(tokens[i] ) ) {
	  // we have a closing symbol
	  if ( tokDebug > 1 ){
	    *Log(theErrLog) << "[detectSentenceBounds] Close FOUND @i=" << i << endl;
	  }
	  //if previous token is EOS and not BOS, it will stop being EOS, as this one will take its place
	  if ((i > 0) && (tokens[i-1].role & ENDOFSENTENCE) && !(tokens[i-1].role & BEGINOFSENTENCE) ) {
	    tokens[i-1].role ^= ENDOFSENTENCE;
	    if (tokens[i].role & BEGINOFSENTENCE) {
	      tokens[i].role ^= BEGINOFSENTENCE;
	    }
	  }
	}
      }
    }
    for (int i = size-1; i > offset; --i ) {
      // at the end of the buffer there may be some PUNCTUATION which
      // has spurious ENDOFSENTENCE and BEGINOFSENTENCE annotation
      // fix this up to avoid sentences containing only punctuation
      if (tokDebug > 1 ){
	*Log(theErrLog) << "[detectSentenceBounds:fixup] i="<< i << " word=["
			<< tokens[i].us
			<< "] type=" << tokens[i].type
			<< ", role=" << tokens[i].role << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ) {
	if (tokens[i].role & BEGINOFSENTENCE) {
	  tokens[i].role ^= BEGINOFSENTENCE;
	}
	if ( i != size-1 ){
	  if (tokens[i].role & ENDOFSENTENCE) {
	    tokens[i].role ^= ENDOFSENTENCE;
	  }
	}
      }
      else
	break;
    }
  }

  void TokenizerClass::detectQuotedSentenceBounds( const int offset ){
    //find sentences
    const int size = tokens.size();
    for (int i = offset; i < size; i++) {
      if (tokDebug > 1 ){
	*Log(theErrLog) << "[detectQuotedSentenceBounds] i="<< i << " word=["
			<< tokens[i].us
			<<"] role=" << tokens[i].role << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ) {
	// we have some kind of punctuation. Does it mark an eos?
	bool is_eos = detectEos( i );
	if (is_eos) {
	  if ( !quotes.emptyStack() ) {
	    if ( tokDebug > 1 ){
	      *Log(theErrLog) << "[detectQuotedSentenceBounds] Preliminary EOS FOUND @i=" << i << endl;
	    }
	    //if there are quotes on the stack, we set a temporary EOS marker, to be resolved later when full quote is found.
	    tokens[i].role |= TEMPENDOFSENTENCE;
	    //If previous token is also TEMPENDOFSENTENCE, it stops being so in favour of this one
	    if ((i > 0) && (tokens[i-1].role & TEMPENDOFSENTENCE))
	      tokens[i-1].role ^= TEMPENDOFSENTENCE;
	  }
	  else if (!sentenceperlineinput)  { //No quotes on stack (and no one-sentence-per-line input)
	    if ( tokDebug > 1 ){
	      *Log(theErrLog) << "[detectQuotedSentenceBounds] EOS FOUND @i=" << i << endl;
	    }
	    tokens[i].role |= ENDOFSENTENCE;
	    //if this is the end of the sentence, the next token is the beginning of a new one
	    if ((i + 1 < size) && !(tokens[i+1].role & BEGINOFSENTENCE))
	      tokens[i+1].role |= BEGINOFSENTENCE;
	    //if previous token is EOS and not BOS, it will stop being EOS, as this one will take its place
	    if ((i > 0) && (tokens[i-1].role & ENDOFSENTENCE) && !(tokens[i-1].role & BEGINOFSENTENCE) ) {
	      tokens[i-1].role ^= ENDOFSENTENCE;
	      if (tokens[i].role & BEGINOFSENTENCE) {
		tokens[i].role ^= BEGINOFSENTENCE;
	      }
	    }
	  }
	}
	else if ( isClosing(tokens[i] ) ) {
	  // we have a closing symbol
	  if ( tokDebug > 1 ){
	    *Log(theErrLog) << "[detectSentenceBounds] Close FOUND @i=" << i << endl;
	  }
	  //if previous token is EOS and not BOS, it will stop being EOS, as this one will take its place
	  if ((i > 0) && (tokens[i-1].role & ENDOFSENTENCE) && !(tokens[i-1].role & BEGINOFSENTENCE) ) {
	    tokens[i-1].role ^= ENDOFSENTENCE;
	    if (tokens[i].role & BEGINOFSENTENCE) {
	      tokens[i].role ^= BEGINOFSENTENCE;
	    }
	  }
	}
	//check quotes
	detectQuoteBounds(i);
      }
    }
  }

  TokenizerClass::~TokenizerClass(){
    for ( unsigned int i = 0; i < rules.size(); i++) {
      delete rules[i];
    }
    rulesmap.clear();
    delete theErrLog;
  }

  void TokenizerClass::passthruLine( const string& s, bool& bos ) {
    UnicodeString input;
    try {
      input = UnicodeString( s.c_str(), s.length(), inputEncoding.c_str() );
    } catch ( exception &e) {
      throw uCodingError( "Unexpected character found in input. " + string(e.what() )
			  + "Make sure input is valid UTF-8!" );
    }
    if ( input.isBogus() ){
      throw uCodingError( "string decoding failed: (invalid inputEncoding '"
			  + inputEncoding + "' ?)" );
    }
    if (tokDebug) {
      *Log(theErrLog) << "[passthruLine] input: line=[" << input << "]" << endl;
    }
    bool alpha = false, num = false, punct = false;
    UnicodeString word;
    StringCharacterIterator sit(input);
    while ( sit.hasNext() ){
      UChar32 c = sit.current32();
      if ( u_isspace(c)) {
	if ( word.isEmpty() ){
	  // a leading space. Don't waste time on it. SKIP
	  sit.next32();
	  continue;
	}
	// so a trailing space. handle the found word.
	if (tokDebug){
	  *Log(theErrLog) << "[passthruLine] word=[" << word << "]" << endl;
	}
	if ( word == eosmark ) {
	  word = "";
	  if (!tokens.empty())
	    tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
	  bos = true;
	}
	else {
	  UnicodeString type;
	  if (alpha && !num && !punct) {
	    type = type_word;
	  }
	  else if (num && !alpha && !punct) {
	    type = type_number;
	  }
	  else if (punct && !alpha && !num) {
	    type = type_punctuation;
	  }
	  else {
	    type = type_unknown;
	  }
	  if ( doPunctFilter
	       && ( type == type_punctuation || type == type_currency ||
		    type == type_emoticon ) ) {
	    if (tokDebug >= 2 ){
	      *Log(theErrLog) << "   [passThruLine] skipped PUNCTUATION ["
			      << input << "]" << endl;
	    }
	    if ( !tokens.empty() && tokens[tokens.size()-1].role & NOSPACE ){
	      tokens[tokens.size()-1].role ^= NOSPACE;
	    }
	  }
	  else {
	    if (bos) {
	      tokens.push_back( Token( type, word , BEGINOFSENTENCE ) );
	      bos = false;
	    }
	    else {
	      tokens.push_back( Token( type, word ) );
	    }
	  }
	  alpha = false;
	  num = false;
	  punct = false;
          word = "";
	}
      }
      else {
	if ( u_isalpha(c)) {
	  alpha = true;
	}
	else if (u_ispunct(c)) {
	  punct = true;
	}
	else if (u_isdigit(c)) {
	  num = true;
	}
	word += c;
      }
      sit.next32();
    }
    if (word != "") {
      if ( word == eosmark ) {
	word = "";
	if (!tokens.empty())
	  tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
      }
      else {
	UnicodeString type;
	if (alpha && !num && !punct) {
	  type = type_word;
	}
	else if (num && !alpha && !punct) {
	  type = type_number;
	}
	else if (punct && !alpha && !num) {
	  type = type_punctuation;
	}
	else {
	  type = type_unknown;
	}
	if ( doPunctFilter
	     && ( type == type_punctuation || type == type_currency ||
		  type == type_emoticon ) ) {
	  if (tokDebug >= 2 ){
	    *Log(theErrLog) << "   [passThruLine] skipped PUNCTUATION ["
			    << input << "]" << endl;
	  }
	  if ( !tokens.empty() && tokens[tokens.size()-1].role & NOSPACE ){
	    tokens[tokens.size()-1].role ^= NOSPACE;
	  }
	}
	else {
	  if (bos) {
	    tokens.push_back( Token( type, word , BEGINOFSENTENCE ) );
	    bos = false;
	  }
	  else {
	    tokens.push_back( Token( type, word ) );
	  }
	}
      }
    }
    if (sentenceperlineinput) {
      tokens[0].role |= BEGINOFSENTENCE;
      tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
    }
  }

  string TokenizerClass::checkBOM( const string& s, string& enc ){
    UErrorCode err = U_ZERO_ERROR;
    int32_t bomLength = 0;
    const char *encoding = ucnv_detectUnicodeSignature( s.c_str(), s.length(),
							&bomLength, &err);
    if ( bomLength ){
      enc = encoding;
      if (tokDebug){
	*Log(theErrLog) << "Autodetected encoding: " << enc << endl;
      }
      return s.substr( bomLength );
    }
    return s;
  }

  // string wrapper
  int TokenizerClass::tokenizeLine( const string& in_s ){
    string s = checkBOM( in_s, inputEncoding );
    UnicodeString uinputstring;
    try {
      uinputstring = UnicodeString( s.c_str(), s.length(), inputEncoding.c_str() );
    } catch ( exception &e) {
      throw uCodingError( "Unexpected character found in input. " + string(e.what() )
			  + "Make sure input is valid UTF-8!" );
    }
    if ( uinputstring.isBogus() ){
      throw uCodingError( "string decoding failed: (invalid inputEncoding '"
			  + inputEncoding + "' ?)" );
    }
    else {
      return tokenizeLine( uinputstring );
    }
  }

  bool u_isemo( UChar32 c ){
    UBlockCode s = ublock_getCode(c);
    return s == UBLOCK_EMOTICONS;
  }

  int TokenizerClass::tokenizeLine( const UnicodeString& originput ){
    if (tokDebug){
      *Log(theErrLog) << "[tokenizeLine] input: line=["
		      << originput << "]" << endl;
    }
    UnicodeString input = normalizer.normalize( originput );
    if ( doFilter ){
      input = filter.filter( input );
    }
    if ( input.isBogus() ){ //only tokenize valid input
      *theErrLog << "ERROR: Invalid UTF-8 in line!:" << input << endl;
      return 0;
    }
    int32_t len = input.countChar32();
    if (tokDebug){
      *Log(theErrLog) << "[tokenizeLine] filtered input: line=["
		      << input << "] (" << len
		      << " unicode characters)" << endl;
    }
    const int begintokencount = tokens.size();
    if (tokDebug) {
      *Log(theErrLog) << "[tokenizeLine] Tokens still in buffer: " << begintokencount << endl;
    }

    bool tokenizeword = false;
    bool reset = false;
    //iterate over all characters
    UnicodeString word;
    StringCharacterIterator sit(input);
    long int i = 0;
    while ( sit.hasNext() ){
      UChar32 c = sit.current32();
      if (reset) { //reset values for new word
	reset = false;
	if (!u_isspace(c))
	  word = c;
	else
	  word = "";
	tokenizeword = false;
      }
      else {
	if (!u_isspace(c)) word += c;
      }
      if ( u_isspace(c) || i == len-1 ){
	if (tokDebug){
	  *Log(theErrLog) << "[tokenizeLine] space detected, word=["
			  << word << "]" << endl;
	}
	if ( i == len-1 ) {
	  if ( u_ispunct(c) || u_isdigit(c) || u_isquote(c) || u_isemo(c) )
	    tokenizeword = true;
	}
	int expliciteosfound = -1;
	if ( word.length() >= eosmark.length() ) {
	  expliciteosfound = word.lastIndexOf(eosmark);

	  if (expliciteosfound != -1) { // word contains eosmark
	    if (tokDebug >= 2){
	      *Log(theErrLog) << "[tokenizeLine] Found explicit EOS marker @"<<expliciteosfound << endl;
	    }
	    int eospos = tokens.size()-1;
	    if (expliciteosfound > 0) {
	      UnicodeString realword;
	      word.extract(0,expliciteosfound,realword);
	      if (tokDebug >= 2) {
		*Log(theErrLog) << "[tokenizeLine] Prefix before EOS: "
				<< realword << endl;
	      }
	      tokenizeWord( realword, false );
	      eospos++;
	    }
	    if (expliciteosfound + eosmark.length() < word.length())  {
	      UnicodeString realword;
	      word.extract(expliciteosfound+eosmark.length(),word.length() - expliciteosfound - eosmark.length(),realword);
	      if (tokDebug >= 2){
		*Log(theErrLog) << "[tokenizeLine] postfix after EOS: "
				<< realword << endl;
	      }
	      tokenizeWord( realword, true );
	    }
	    if ( !tokens.empty() && eospos >= 0 ) {
	      if (tokDebug >= 2){
		*Log(theErrLog) << "[tokenizeLine] Assigned EOS" << endl;
	      }
	      tokens[eospos].role |= ENDOFSENTENCE;
	    }
	  }
	}
	if ((word.length() > 0) && (expliciteosfound == -1)) {
	  if (!tokenizeword) {
	    //single character or nothing tokenisable found, so no need to tokenize anything
	    if (tokDebug >= 2){
	      *Log(theErrLog) << "[tokenizeLine] Word ok, no need for further tokenisation for: ["
			      << word << "]" << endl;;
	    }
	    tokens.push_back( Token( type_word, word ) );
	  }
	  else {
	    if (tokDebug >= 2){
	      *Log(theErrLog) << "[tokenizeLine] Further tokenisation necessary for: ["
			      << word << "]" << endl;
	    }
	    tokenizeWord( word, true );
	  }
	}
	//reset values for new word
	reset = true;
      }
      else if ( u_ispunct(c) || u_isdigit(c) || u_isquote(c) || u_isemo(c) ){
	if (tokDebug){
	  *Log(theErrLog) << "[tokenizeLine] punctuation or digit detected, word=["
			  << word << "]" << endl;
	}
	//there is punctuation or digits in this word, mark to run through tokeniser
	tokenizeword = true;
      }
      sit.next32();
      ++i;
    }
    int numNewTokens = tokens.size() - begintokencount;
    if ( numNewTokens > 0 ){
      if (paragraphsignal) {
	tokens[begintokencount].role |= NEWPARAGRAPH | BEGINOFSENTENCE;
	paragraphsignal = false;
      }
      if ( detectBounds ){
	//find sentence boundaries
	if (sentenceperlineinput) {
	  tokens[begintokencount].role |= BEGINOFSENTENCE;
	  tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
	  if ( detectQuotes ){
	    detectQuotedSentenceBounds( begintokencount );
	  }
	}
	else {
	  if ( detectQuotes ){
	    detectQuotedSentenceBounds( begintokencount );
	  }
	  else {
	    detectSentenceBounds( begintokencount );
	  }
	}
      }
    }
    return numNewTokens;
  }

  bool Rule::matchAll( const UnicodeString& line,
		       UnicodeString& pre,
		       UnicodeString& post,
		       vector<UnicodeString>& matches ){
    matches.clear();
    pre = "";
    post = "";
#ifdef MATCH_DEBUG
    cerr << "match: " << id << endl;
#endif
    if ( regexp && regexp->match_all( line, pre, post ) ){
      int num = regexp->NumOfMatches();
      if ( num >=1 ){
	for( int i=1; i <= num; ++i ){
	  matches.push_back( regexp->get_match( i ) );
	}
      }
      else {
	matches.push_back( regexp->get_match( 0 ) );
      }
      return true;
    }
    return false;
  }

  void TokenizerClass::tokenizeWord( const UnicodeString& input, bool space ) {
    int32_t inpLen = input.countChar32();
    if ( tokDebug > 2 ){
      *Log(theErrLog) << "   [tokenizeWord] Input: (" << inpLen << ") "
		      << "word=[" << input << "]" << endl;
    }
    if ( input == eosmark ) {
      if (tokDebug >= 2){
	*Log(theErrLog) << "   [tokenizeWord] Found explicit EOS marker" << endl;
      }
      if (!tokens.empty()) {
	if (tokDebug >= 2){
	  *Log(theErrLog) << "   [tokenizeWord] Assigned EOS" << endl;
	}
	tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
      }
      else {
	*Log(theErrLog) << "[WARNING] Found explicit EOS marker by itself, this will have no effect!" << endl;
      }
      return;
    }

    if ( inpLen == 1) {
      //single character, no need to process all rules, do some simpler (faster) detection
      UChar32 c = input.char32At(0);
      UnicodeString type;

      if ( u_ispunct(c)) {
	if (  u_charType( c ) == U_CURRENCY_SYMBOL ) {
	  type = type_currency;
	}
	else {
	  type = type_punctuation;
	}
      }
      else if ( u_isemo( c ) ) {
	type = type_emoticon;
      }
      else if ( u_isalpha(c)) {
	type = type_word;
      }
      else if ( u_isdigit(c)) {
	type = type_number;
      }
      else if ( u_isspace(c)) {
	return;
      }
      else {
	if ( u_charType( c ) == U_CURRENCY_SYMBOL ) {
	  type = type_currency;
	}
	else {
	  type = type_unknown;
	}
      }
      if ( doPunctFilter
	   && ( type == type_punctuation || type == type_currency ||
		type == type_emoticon ) ) {
	if (tokDebug >= 2 ){
	  *Log(theErrLog) << "   [tokenizeWord] skipped PUNCTUATION ["
			  << input << "]" << endl;
	}
	if ( !tokens.empty() && tokens[tokens.size()-1].role & NOSPACE ){
	  tokens[tokens.size()-1].role ^= NOSPACE;
	}
      }
      else {
	Token T( type, input, space ? NOROLE : NOSPACE );
	tokens.push_back( T );
	if (tokDebug >= 2){
	  *Log(theErrLog) << "   [tokenizeWord] added token " << T << endl;
	}
      }
    }
    else {
      for ( unsigned int i = 0; i < rules.size(); i++) {
	if ( tokDebug >= 4){
	  *Log(theErrLog) << "\tTESTING " << rules[i]->id << endl;
	}
	//Find first matching rule
	UnicodeString pre, post;
	vector<UnicodeString> matches;
	if ( rules[i]->matchAll( input, pre, post, matches ) ){
	  if ( tokDebug >= 4 ){
	    *Log(theErrLog) << "\tMATCH: " << rules[i]->id << endl;
	  }
	  if ( pre.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      *Log(theErrLog) << "\tTOKEN pre-context (" << pre.length()
			      << "): [" << pre << "]" << endl;
	    }
	    tokenizeWord( pre, false ); //pre-context, no space after
	  }
	  if ( matches.size() > 0 ){
	    int max = matches.size();
	    if ( tokDebug >= 4 ){
	      *Log(theErrLog) << "\tTOKEN match #=" << matches.size() << endl;
	    }
	    for ( int m=0; m < max; ++m ){
	      if ( tokDebug >= 4 ){
		*Log(theErrLog) << "\tTOKEN match[" << m << "] = "
				<< matches[m] << endl;
	      }
	      if ( doPunctFilter
		   && (&rules[i]->id)->startsWith("PUNCTUATION") ){
		if (tokDebug >= 2 ){
		  *Log(theErrLog) << "   [tokenizeWord] skipped PUNCTUATION ["
				  << matches[m] << "]" << endl;
		}
		if ( !tokens.empty() && tokens[tokens.size()-1].role & NOSPACE ){
		  tokens[tokens.size()-1].role ^= NOSPACE;
		}
	      }
	      else {
		if ( post.length() > 0 ) space = false;
		tokens.push_back( Token( rules[i]->id, matches[m], space ? NOROLE : NOSPACE ) );
	      }
	    }
	  }
	  else if ( tokDebug >=4 ){
	    *Log(theErrLog) << "\tthere's no match" << endl;
	  }
	  if ( post.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      *Log(theErrLog) << "\tTOKEN post-context (" << post.length()
			      << "): [" << post << "]" << endl;
	    }
	    tokenizeWord( post, space ? NOROLE : NOSPACE );
	  }
	  break;
	}
      }
    }
  }

  bool TokenizerClass::readrules( const string& fname) {
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = folia::UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    *theErrLog << "include line = " << rawline << endl;
	  }
	  const int splitpoint = line.indexOf("=");
	  if ( splitpoint < 0 ){
	    throw uConfigError( "invalid RULES entry: " + line );
	  }
	  UnicodeString id = UnicodeString( line, 0,splitpoint);
	  UnicodeString pattern = UnicodeString( line, splitpoint+1);
	  rulesmap[id] = new Rule( id, pattern);
	}
      }
    }
    return true;
  }

  bool TokenizerClass::readfilters( const string& fname) {
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    return filter.fill( fname );
  }

  bool TokenizerClass::readquotes( const string& fname) {
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = folia::UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    *theErrLog << "include line = " << rawline << endl;
	  }
	  int splitpoint = line.indexOf(" ");
	  if ( splitpoint == -1 )
	    splitpoint = line.indexOf("\t");
	  if ( splitpoint == -1 ){
	    throw uConfigError( "invalid QUOTES entry: " + line
				+ " (missing whitespace)" );
	  }
	  UnicodeString open = UnicodeString( line, 0,splitpoint);
	  UnicodeString close = UnicodeString( line, splitpoint+1);
	  open = open.trim().unescape();
	  close = close.trim().unescape();
	  if ( open.isEmpty() || close.isEmpty() ){
	    throw uConfigError( "invalid QUOTES entry: " + line );
	  }
	  else {
	    quotes.add( open, close );
	  }
	}
      }
    }
    return true;
  }

  bool TokenizerClass::readeosmarkers( const string& fname) {
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = folia::UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    *theErrLog << "include line = " << rawline << endl;
	  }
	  if ( ( line.startsWith("\\u") && line.length() == 6 ) ||
	       ( line.startsWith("\\U") && line.length() == 10 ) ){
	    UnicodeString uit = line.unescape();
	    if ( uit.isEmpty() ){
	      throw uConfigError( "Invalid EOSMARKERS entry: " + line );
	    }
	    eosmarkers += uit;
	  }
	}
      }
    }
    return true;
  }

  bool TokenizerClass::readabbreviations( const string& fname,
					  UnicodeString& abbreviations ) {
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = folia::UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    *theErrLog << "include line = " << rawline << endl;
	  }
	  if ( !abbreviations.isEmpty())
	    abbreviations += '|';
	  abbreviations += line;
	}
      }
    }
    return true;
  }

  ConfigMode getMode( const UnicodeString& line ) {
    ConfigMode mode = NONE;
    if (line == "[RULES]") {
      mode = RULES;
    }
    else if (line == "[META-RULES]") {
      mode = METARULES;
    }
    else if (line == "[RULE-ORDER]") {
      mode = RULEORDER;
    }
    else if (line == "[ABBREVIATIONS]") {
      mode = ABBREVIATIONS;
    }
    else if (line == "[ATTACHEDPREFIXES]") {
      mode = ATTACHEDPREFIXES;
    }
    else if (line == "[ATTACHEDSUFFIXES]") {
      mode = ATTACHEDSUFFIXES;
    }
    else if (line == "[PREFIXES]") {
      mode = PREFIXES;
    }
    else if (line == "[SUFFIXES]") {
      mode = SUFFIXES;
    }
    else if (line == "[TOKENS]") {
      mode = TOKENS;
    }
    else if (line == "[UNITS]") {
      mode = UNITS;
    }
    else if (line == "[ORDINALS]") {
      mode = ORDINALS;
    }
    else if (line == "[EOSMARKERS]") {
      mode = EOSMARKERS;
    }
    else if (line == "[QUOTES]") {
      mode = QUOTES;
    }
    else if (line == "[FILTER]") {
      mode = FILTER;
    }
    return mode;
  }

  void addOrder( vector<UnicodeString>& order, UnicodeString &line ){
    try {
      UnicodeRegexMatcher m( "\\s+" );
      vector<UnicodeString> tmp;
      int num = m.split( line, tmp );
      for ( int i=0; i < num; ++i )
	order.push_back( tmp[i] );
    }
    catch ( exception& e ){
      abort();
    }
  }

  void TokenizerClass::sortRules( map<UnicodeString, Rule *>& rulesmap,
				  vector<Rule *>& result,
				  const vector<UnicodeString>& sort ){
    // *Log(theErrLog) << "rules voor sort : " << endl;
    // for ( size_t i=0; i < rules.size(); ++i ){
    //   *Log(theErrLog) << "rule " << i << " " << *rules[i] << endl;
    // }
    if ( !sort.empty() ){
      for ( auto const& id : sort ){
	auto it = rulesmap.find( id );
	if ( it != rulesmap.end() ){
	  result.push_back( it->second );
	  rulesmap.erase( it );
	}
	else {
	  *Log(theErrLog) << "RULE-ORDER specified for undefined RULE '"
			  << id << "'" << endl;
	}
      }
      for ( auto const& it : rulesmap ){
	*Log(theErrLog) << "No RULE-ORDER specified for RULE '"
			<< it.first << "' (put at end)." << endl;
	result.push_back( it.second );
      }
    }
    else {
      for ( auto const& it : rulesmap ){
	result.push_back( it.second );
      }
    }
    // *Log(theErrLog) << "rules NA sort : " << endl;
    // for ( size_t i=0; i < result.size(); ++i ){
    //   *Log(theErrLog) << "rule " << i << " " << *result[i] << endl;
    // }
  }

  void TokenizerClass::add_rule( const UnicodeString& name,
				 const vector<string>& parts,
				 const UnicodeString& list ){
    UnicodeString pat = folia::UTF8ToUnicode( parts[0] );
    pat += list;
    pat += folia::UTF8ToUnicode( parts[2] );
    rulesmap[name] = new Rule( name, pat );
  }

  bool TokenizerClass::readsettings( const string& fname ) {

    ConfigMode mode = NONE;

    map<ConfigMode, UnicodeString> pattern = { { ABBREVIATIONS, "" },
					       { TOKENS, "" },
					       { PREFIXES, "" },
					       { SUFFIXES, "" },
					       { ATTACHEDPREFIXES, "" },
					       { ATTACHEDSUFFIXES, "" },
					       { UNITS, "" },
					       { ORDINALS, "" } };

    vector<UnicodeString> rules_order;
    vector<string> meta_rules;

    string confdir;
    string conffile;
    if ( fname.find_first_of( "/" ) != string::npos ){
      // override 'system' dir when cfile seems a relative or absolute path
      string::size_type pos = fname.rfind("/");
      confdir = fname.substr( 0, pos+1 );
      conffile = fname;
    }
    else {
      confdir = defaultConfigDir;
      conffile = confdir + fname;
    }
    ifstream f( conffile );
    if ( !f ){
      return false;
    }
    else {
      if ( tokDebug ){
	*Log(theErrLog) << "config directory=" << confdir << endl;
	*Log(theErrLog) << "config file=" << conffile << endl;
      }
      string rawline;
      while ( getline(f,rawline) ){
	if ( rawline.find( "%include" ) != string::npos ){
	  string file = rawline.substr( 9 );
	  if ( file.find_first_of( "/" ) != string::npos ){
	    // a relative or absolute path. do not touch
	  }
	  else {
	    file = confdir + file;
	  }
	  switch ( mode ){
	  case RULES: {
	    file += ".rule";
	    if ( !readrules( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case FILTER:{
	    file += ".filter";
	    if ( !readfilters( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case QUOTES:{
	    file += ".quote";
	    if ( !readquotes( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case EOSMARKERS:{
	    file += ".eos";
	    if ( !readeosmarkers( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case ABBREVIATIONS:{
	    file += ".abr";
	    if ( !readabbreviations( file, pattern[ABBREVIATIONS] ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  default:
	    throw uConfigError( string("%include not implemented for this section" ) );
	  }
	  continue;
	}

	UnicodeString line = folia::UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if (line[0] == '[') {
	    mode = getMode( line );
	  }
	  else {
	    if ( line[0] == '\\' && line.length() > 1 && line[1] == '[' ){
	      line = UnicodeString( line, 1 );
	    }
	    switch( mode ){
	    case RULES: {
	      const int splitpoint = line.indexOf("=");
	      if ( splitpoint < 0 ){
		throw uConfigError( "invalid RULES entry: " + line );
	      }
	      UnicodeString id = UnicodeString( line, 0,splitpoint);
	      UnicodeString pattern = UnicodeString( line, splitpoint+1);
	      rulesmap[id] = new Rule( id, pattern);
	    }
	      break;
	    case RULEORDER:
	      addOrder( rules_order, line );
	      break;
	    case METARULES:
	      meta_rules.push_back( folia::UnicodeToUTF8(line) );
	      break;
	    case ABBREVIATIONS:
	    case ATTACHEDPREFIXES:
	    case ATTACHEDSUFFIXES:
	    case PREFIXES:
	    case SUFFIXES:
	    case TOKENS:
	    case UNITS:
	    case ORDINALS:
	      if ( !pattern[mode].isEmpty() )
		pattern[mode] += '|';
	      pattern[mode] += line;
	      break;
	    case EOSMARKERS:
	      if ( ( line.startsWith("\\u") && line.length() == 6 ) ||
		   ( line.startsWith("\\U") && line.length() == 10 ) ){
		UnicodeString uit = line.unescape();
		if ( uit.isEmpty() ){
		  throw uConfigError( "Invalid EOSMARKERS entry: " + line );
		}
		eosmarkers += uit;
	      }
	      break;
	    case QUOTES: {
	      int splitpoint = line.indexOf(" ");
	      if ( splitpoint == -1 )
		splitpoint = line.indexOf("\t");
	      if ( splitpoint == -1 ){
		throw uConfigError( "invalid QUOTES entry: " + line
				    + " (missing whitespace)" );
	      }
	      UnicodeString open = UnicodeString( line, 0,splitpoint);
	      UnicodeString close = UnicodeString( line, splitpoint+1);
	      open = open.trim().unescape();
	      close = close.trim().unescape();
	      if ( open.isEmpty() || close.isEmpty() ){
		throw uConfigError( "invalid QUOTES entry: " + line );
	      }
	      else {
		quotes.add( open, close );
	      }
	    }
	      break;
	    case FILTER:
	      filter.add( line );
	      break;
	    case NONE:
	      break;
	    default:
	      throw uLogicError("unhandled case in switch");
	    }
	  }
	}
      }
    }

    // set reasonable defaults for those items that ar NOT set
    // in the configfile
    if ( eosmarkers.length() == 0 ){
      eosmarkers = ".!?";
    }
    if ( quotes.empty() ){
      quotes.add( '"', '"' );
      quotes.add( "‘", "’" );
      quotes.add( "“„‟", "”" );
    }

    string split = "%";
    // Create Rules for every pattern that is set
    // first the meta rules...
    for ( const auto& mr : meta_rules ){
      string::size_type pos = mr.find( "=" );
      if ( pos == string::npos ){
	throw uConfigError( "invalid entry in META-RULES: " + mr );
      }
      string nam = TiCC::trim( mr.substr( 0, pos ) );
      if ( nam == "SPLITTER" ){
	split = mr.substr( pos+1 );
	if ( split.empty() ) {
	  throw uConfigError( "invalid SPLITTER value in META-RULES: " + mr );
	}
	if ( split[0] == '"' && split[split.length()-1] == '"' ){
	  split = split.substr(1,split.length()-2);
	}
	if ( tokDebug > 5 ){
	  *Log(theErrLog) << "SET SPLIT: '" << split << "'" << endl;
	}
	continue;
      }
      UnicodeString name = folia::UTF8ToUnicode( nam );
      string rule = mr.substr( pos+1 );
      if ( tokDebug > 5 ){
	*Log(theErrLog) << "SPLIT using: '" << split << "'" << endl;
      }
      vector<string> parts;
      size_t num = TiCC::split_at( rule, parts, split );
      if ( num != 3 ){
	throw uConfigError( "invalid entry in META-RULES: " + mr + " 3 parts expected" );
      }
      for ( auto& str : parts ){
	str = TiCC::trim( str );
      }
      UnicodeString meta =  folia::UTF8ToUnicode( parts[1] );
      ConfigMode mode = getMode( "[" + meta + "]" );
      if ( mode == NONE ){
	throw uConfigError( "invalid REFERENCE '" + meta + "' in META-RULE: "
			    + folia::UTF8ToUnicode(mr) );
      }
      switch ( mode ){
      case ORDINALS:
      case ABBREVIATIONS:
      case TOKENS:
      case ATTACHEDPREFIXES:
      case ATTACHEDSUFFIXES:
      case UNITS:
      case PREFIXES:
      case SUFFIXES:
	if ( !pattern[mode].isEmpty()){
	  add_rule( name, parts, pattern[mode] );
	}
	break;
      default:
	break;
      }
    }

    // old style /defaultstuff...
    if ( rulesmap.find( "NUMBER-ORDINAL" ) == rulesmap.end()
	 && !pattern[ORDINALS].isEmpty() ){
      rulesmap["NUMBER-ORDINAL"] = new Rule("NUMBER-ORDINAL", "\\p{N}+-?(?:" + pattern[ORDINALS] + ")(?:\\Z|\\P{Lu}|\\P{Ll})$");
      ////
      // NB: (?i) is not used for the whole expression because of icu bug 8824
      //     see http://bugs.icu-project.org/trac/ticket/8824
      //     If you want mixed case Ordinals, you have to enumerate them all
      //     in the config file
    }
    /* if ( rulesmap.find( "UNIT" ) == rulesmap.end()
       && !pattern[UNITS].empty() ){
       rulesmap["UNIT"] = new Rule("UNIT", "(?i)(?:\\a|\\P{L})(" + pattern[UNITS] + ")(?:\\z|\\P{L})");
       }
    */
    if ( rulesmap.find( "ABBREVIATION-KNOWN" ) == rulesmap.end()
	 && !pattern[ABBREVIATIONS].isEmpty() ){
      rulesmap["ABBREVIATION-KNOWN"] = new Rule("ABBREVIATION-KNOWN",  "(?:\\p{P}*)?(?:\\A|[^\\p{L}\\.])((?:" + pattern[ABBREVIATIONS] + ")\\.)(?:\\Z|\\P{L})");
    }
    if ( rulesmap.find( "WORD-TOKEN" ) == rulesmap.end()
	 && !pattern[TOKENS].isEmpty() ){
      rulesmap["WORD-TOKEN"] = new Rule("WORD-TOKEN", "(" + pattern[TOKENS] + ")(?:\\p{P}*)?$");
    }
    if ( rulesmap.find( "WORD-WITHPREFIX" ) == rulesmap.end()
	 && !pattern[ATTACHEDPREFIXES].isEmpty() ){
      rulesmap["WORD-WITHPREFIX"] = new Rule("WORD-WITHPREFIX", "(?:\\A|[^\\p{Lu}\\.]|[^\\p{Ll}\\.])(?:" + pattern[ATTACHEDPREFIXES] + ")\\p{L}+");
    }
    if ( rulesmap.find( "WORD-WITHSUFFIX" ) == rulesmap.end()
	 && !pattern[ATTACHEDSUFFIXES].isEmpty() ){
      rulesmap["WORD-WITHSUFFIX"] = new Rule("WORD-WITHSUFFIX", "((?:\\p{L}|\\p{N}|-)+(?:" + pattern[ATTACHEDSUFFIXES] + "))(?:\\Z|\\p{P})");
    }
    if ( rulesmap.find( "PREFIX" ) == rulesmap.end()
	 && !pattern[PREFIXES].isEmpty() ){
      rulesmap["PREFIX"] = new Rule("PREFIX", "(?:\\A|[^\\p{Lu}\\.]|[^\\p{Ll}\\.])(" + pattern[PREFIXES] + ")(\\p{L}+)");
    }
    if ( rulesmap.find( "SUFFIX" ) == rulesmap.end()
	 && !pattern[SUFFIXES].isEmpty() ){
      rulesmap["SUFFIX"] = new Rule("SUFFIX", "((?:\\p{L})+)(" + pattern[SUFFIXES] + ")(?:\\Z|\\P{L})");
      //adding (?i) causes RegexMatcher->find() to get caught in an endless loop :(
    }
    sortRules( rulesmap, rules, rules_order );
    return true;
  }

  bool TokenizerClass::reset(){
    tokens.clear();
    quotes.clearStack();
    return true;
  }

  bool TokenizerClass::init( const string& fname ){
    *Log(theErrLog) << "Initiating tokeniser..." << endl;
    if (!readsettings( fname ) ) {
      throw uConfigError( "Cannot read Tokeniser settingsfile "+ fname );
      return false;
    }
    settingsfilename = fname;
    if ( tokDebug ){
      *Log(theErrLog) << "effective rules: " << endl;
      for ( size_t i=0; i < rules.size(); ++i ){
	*Log(theErrLog) << "rule " << i << " " << *rules[i] << endl;
      }
      *Log(theErrLog) << "EOS markers: " << eosmarkers << endl;
      *Log(theErrLog) << "Quotations: " << quotes << endl;
      *Log(theErrLog) << "Filter: " << filter << endl;
    }
    return true;
  }

}//namespace
