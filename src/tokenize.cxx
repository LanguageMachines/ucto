/*
  $Id$
  $URL$
  Copyright (c) 2006 - 2011
  Tilburg University

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

  For more information and updates, see:
      http://ilk.uvt.nl/frog
*/

/* ************************************
* 
*  Original version by Maarten van Gompel, ILK, Tilburg University
*     proycon AT anaproy DOT nl
*
*     Tilburg University
*
*     Licensed under GPLv3
*
*     v0.2 - 2010-05-11
*     v0.3 - 2010-05-19 - Migration from GLIB to ICU, by Ko van der Sloot
*     v0.4 - 2010-12-10 - promoted into a separate module
*
************************************ */

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include "unicode/ustream.h"
#include "unicode/regex.h"
#include "ucto/unicode.h"
#include "config.h"
#include "ucto/tokenize.h"

using namespace std;

#define Log 

namespace Tokenizer {

  std::string Version() { return VERSION; };
  std::string VersionName() { return PACKAGE_STRING; };

  
  enum ConfigMode { NONE, RULES, ABBREVIATIONS, ATTACHEDPREFIXES, ATTACHEDSUFFIXES, PREFIXES, SUFFIXES, TOKENS, UNITS, ORDINALS, EOSMARKERS, QUOTES, FILTER, RULEORDER };
  
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
    uConfigError( const UnicodeString& us ): invalid_argument( "ucto: config file :" + UnicodeToUTF8(us) ){};
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
      failString = "Invalid regular expression '" + UnicodeToUTF8(pat) +
	"', no compiling possible";	
      throw uConfigError(failString);
    }
    else {
      matcher = pattern->matcher( u_stat );
      if (U_FAILURE(u_stat)){
	failString = "unable to create PatterMatcher with pattern '" + 
	  UnicodeToUTF8(pat) + "'";
	throw uConfigError(failString);
      }
    }
  }
  
  UnicodeRegexMatcher::~UnicodeRegexMatcher(){
    delete pattern;
    delete matcher;
  }
  
  bool UnicodeRegexMatcher::match_all( const UnicodeString& line, 
				       UnicodeString& pre,
				       UnicodeString& post ){
    UErrorCode u_stat = U_ZERO_ERROR;
    pre = "";
    post = "";
    results.clear();
    if ( matcher ){
      matcher->reset( line );
      if ( matcher->find() ){
	// cerr << "matched " << UnicodeToUTF8(line) << endl;
	int start = -1;
	int end = 0;
	for ( int i=0; i <= matcher->groupCount(); ++i ){
	  // cerr << "group " << i << endl;
	  u_stat = U_ZERO_ERROR;
	  start = matcher->start( i, u_stat );
	  if (!U_FAILURE(u_stat)){
	    if ( start < 0 ){
	      continue;
	    }
	  }
	  else
	    break;
	  if ( start > end ){
	    pre = UnicodeString( line, end, start );
	    // cerr << "found pre " << UnicodeToUTF8(pre) << endl;
	  }
	  end = matcher->end( i, u_stat );
	  if (!U_FAILURE(u_stat)){
	    results.push_back( UnicodeString( line, start, end - start ) );
	    // cerr << "added result " << UnicodeToUTF8( results[results.size()-1] ) << endl;
	  }
	  else
	    break;
	}
	if ( end < line.length() ){
	  post = UnicodeString( line, end );
	  // cerr << "found post " << UnicodeToUTF8(post) << endl;
	}
	return true;
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
  const UnicodeString type_word = "WORD";
  const UnicodeString type_punctuation = "PUNCTUATION";
  const UnicodeString type_number = "NUMBER";
  const UnicodeString type_unknown = "UNKNOWN";

  const UnicodeString explicit_eos_marker = "<utt>";

  ostream& operator<<( ostream& os, const Quoting& q ){
    for( size_t i=0; i < q.quotes.size(); ++i ){
      os << UnicodeToUTF8(q.quotes[i].openQuote) 
	 << "\t" << UnicodeToUTF8( q.quotes[i].closeQuote ) << endl;
    }
    return os;
  }

  void Quoting::flushStack( int beginindex ) {
    //flush up to (but not including) the specified index
    if ( !quotestack.empty() ){
      std::vector<int> new_quoteindexstack;
      std::vector<UChar> new_quotestack;
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
    int res = -1;
    for ( size_t i = 0; i < quotestack.size(); ++i ) {
      if ( open.indexOf( quotestack[i] ) >= 0 ){
	stackindex = i;
	return quoteindexstack[i];
      }
    }
    return res;
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

  Token::Token( const UnicodeString *_type, 
		const UnicodeString& _s,
		TokenRole _role): type(_type), us(_s), role(_role) {}
  

  ostream& operator<< (std::ostream& os, const Token& t ){
    os << UnicodeToUTF8( *t.type ) << " : " << t.role 
       << ":" << UnicodeToUTF8( t.us );
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
      os << UnicodeToUTF8( r.id ) << "=\"" << UnicodeToUTF8( r.regexp->Pattern() ) << "\"";
    }
    else
      os << UnicodeToUTF8( r.id ) << "NULL";
    return os;
  }

  TokenizerClass::TokenizerClass():
    inputEncoding( "UTF-8" ), eosmark("<utt>"), 
    theErrLog(&cerr), 
    tokDebug(0), verbose(false), 
    detectBounds(true), detectQuotes(true), doFilter(true), detectPar(true),
    paragraphsignal(true),sentencesignal(true),
    sentenceperlineoutput(false), sentenceperlineinput(false), 
    lowercase(false), uppercase(false), 
    xmlout(false), passthru(false)
  {}

  string TokenizerClass::setInputEncoding( const std::string& enc ){
    string old = inputEncoding;
    inputEncoding = enc;
    return old;
  }
  
  void TokenizerClass::tokenize( istream& IN, ostream& OUT) {
      bool firstoutput = true;
      bool in_paragraph = false; //for XML
      bool done = false;
      bool bos = true;
      if (xmlout) outputXMLHeader( OUT );      
      string line;      
      do {	    
	done = !getline( IN, line );
	if ( sentenceperlineinput )
	  line += string(" ") + UnicodeToUTF8(explicit_eos_marker);
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
	  if  (tokDebug > 0) *Log(theErrLog) << "[tokenize] " << numS << " sentence(s) in buffer, processing..." << endl;
	  for (int i = 0; i < numS; i++) {
	    int begin, end;
	    if (!getSentence(i, begin, end)) {
          if  (tokDebug > 0) *Log(theErrLog) << "[tokenize] ERROR: Sentence index " << i << " is out of range!!" << endl;
	      throw uRangeError("Sentence index"); //should never happen
	    }
	    /* ******* Begin process sentence  ********** */
	    if (tokDebug >= 1) *Log(theErrLog) << "[tokenize] Outputting sentence " << i << ", begin="<<begin << ",end="<< end << endl;
	    if (xmlout) {
	      outputTokensXML(OUT, begin, end, in_paragraph );	    
	    } else {
	      outputTokens(OUT, begin, end, firstoutput );
	      firstoutput = false;
	    }	       
	  }
	  //clear processed sentences from buffer
	  if  (tokDebug > 0) *Log(theErrLog) << "[tokenize] flushing " << numS << " sentence(s) from buffer..." << endl;
	  flushSentences(numS);	    
	} else {
	  if  (tokDebug > 0) *Log(theErrLog) << "[tokenize] No sentences yet, reading on..." << endl;
	}	
      } while (!done);
      if (xmlout) {
	outputXMLFooter(OUT, in_paragraph);
      } else {
	OUT << endl;
      }
  }
  
  void TokenizerClass::outputXMLHeader( ostream& OUT) {
    OUT << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<?xml-stylesheet type=\"text/xsl\" href=\"folia.xsl\"?>\n"
      //TODO: add metadata support: xmlns:imdi="http://www.mpi.nl/IMDI/Schema/IMDI"
	<< "<FoLiA xmlns=\"http://ilk.uvt.nl/folia\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xml:id=\"" + docid + "\">\n"
    << " <metadata>\n"
	<< "  <annotations>\n"
	<< "   <token-annotation set=\"" << settingsfilename << "\" annotator=\"ucto\" annotatortype=\"auto\" />\n"
	<< "  </annotations>\n"
    << " </metadata>\n"
	<< " <text xml:id=\"" << docid << ".text\">\n";
    count_p = count_s = count_w = 0;
  }
  
  void TokenizerClass::outputXMLFooter( ostream& OUT, bool in_paragraph) {
    if (in_paragraph) 
      OUT << "    </p>\n </text>\n</FoLiA>\n";
  }
  
  void TokenizerClass::outputTokensXML( ostream& OUT, 
					const size_t begin, const size_t end, 
					bool& in_paragraph ) {	
    short count_q = 0;
    short count_qs = 0;
    short quotelevel = 0;
    if (end >= tokens.size()) {
      throw uRangeError("End index for outputTokensXML exceeds available buffer length" );
    }
    for ( size_t i = begin; i <= end; i++) {
      if ((tokens[i].role & NEWPARAGRAPH) || (!in_paragraph)) {	    
	if (in_paragraph) 
	  OUT << "    </p>" << endl;
	count_p++;
	count_s = 0;
	count_w = 0;
	quotelevel = 0;
	OUT << "    <p xml:id=\"" << docid << ".p." << count_p << "\">" << endl;
      }
      if (tokens[i].role & ENDQUOTE) {
	OUT << "      </quote>\n";
	quotelevel--;
      }
      if (tokens[i].role & BEGINOFSENTENCE) {
	if (quotelevel == 0) {
	  count_s++;
	  count_w = 0;
	  OUT << "     <s xml:id=\""<< docid << ".p." << count_p 
	      << ".s." << count_s << "\">" << endl;	    
	} else {
	  count_qs++;
	  OUT << "        <s xml:id=\""<< docid << ".p." << count_p 
	      << ".s." << count_s << ".quote." << count_q 
	      << ".s." << count_qs << "\">" << endl;	    
	}
      }	
      count_w++;	
      if (quotelevel > 0) OUT << "     ";
      if (tokens[i].role & NOSPACE) {
	OUT << "      <w xml:id=\"" << docid << ".p." << count_p 
	    << ".s." << count_s << ".w." << count_w 
	    << "\" class=\"" + UnicodeToUTF8( *tokens[i].type ) + "\" space=\"no\"><t>" 
	    << UnicodeToUTF8(tokens[i].us) << "</t></w>" << endl;	    
      } else {
	OUT << "      <w xml:id=\"" << docid << ".p." << count_p 
	    << ".s." << count_s << ".w." << count_w 
	    << "\" class=\"" + UnicodeToUTF8(*tokens[i].type ) + "\"><t>" 
	    << UnicodeToUTF8(tokens[i].us) << "</t></w>" << endl;	    
      }
      if (tokens[i].role & BEGINQUOTE) {
	count_q++;
	OUT << "      <quote xml:id=\"" << docid << ".p." << count_p 
	    << ".s." << count_s << ".quote." << count_q << "\">" << endl;
	quotelevel++;
      }    
      if (tokens[i].role & ENDOFSENTENCE) {
	if (quotelevel > 0)
	  OUT << "   ";
	OUT << "     </s>" << endl;
      }
      in_paragraph = true;
    }
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

  void TokenizerClass::outputTokens( ostream& OUT, 
				     const size_t begin, const size_t end, 
				     const bool firstoutput ) const {
    short quotelevel = 0;
    if (end >= tokens.size()) {
      throw uRangeError( "End index for outputTokens exceeds available buffer length" );
    }
    for ( size_t i = begin; i <= end; i++) {
      if ((detectPar) && (tokens[i].role & NEWPARAGRAPH) && (!verbose) && (!firstoutput)) {
	if (sentenceperlineoutput) {
	  OUT << "\n";
	} else {
	  OUT << "\n\n";
	}
      }
      if (lowercase) {
	UnicodeString s = tokens[i].us;
	OUT << UnicodeToUTF8( s.toLower() );
      } else if (uppercase) {
	UnicodeString s = tokens[i].us;
	OUT << UnicodeToUTF8( s.toUpper() );
      } else {
	OUT << UnicodeToUTF8( tokens[i].us );
      }      
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (verbose) {
	OUT << "\t" +  UnicodeToUTF8( *tokens[i].type ) + "\t" 
	    << tokens[i].role << endl;
      }
      if (tokens[i].role & ENDQUOTE) quotelevel--;
      
      if (quotelevel == 0) {
	if (tokens[i].role & ENDOFSENTENCE) {
	  if (verbose) {
	    OUT << "\n";
	  } else {
	    if (sentenceperlineoutput) {
	      OUT << "\n";
	    } else {
	      UnicodeString tmp = UTF8ToUnicode( eosmark );
	      OUT << " " + tmp;
	    }
	  }
	}
      }
      if ( (i <= end) && (!verbose) ) {
	if (!( (tokens[i].role & ENDOFSENTENCE) && (sentenceperlineoutput) )) {
	  OUT << " ";  
	}
      }
    } 
  }
  
  vector<string> TokenizerClass::getSentences() const {
    short quotelevel = 0;
    vector<string> sentences;
    const int size = tokens.size();
    string sentence = "";
    for (int i = 0; i < size; i++) {
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;	
      sentence += UnicodeToUTF8(tokens[i].us);
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	sentence += " " + eosmark;
	sentences.push_back(sentence);
	sentence = "";
      } else if (i < size) {
	sentence += " ";
      }
    }
    if (!sentence.empty()) {
      sentences.push_back(sentence);
    }      
    return sentences;     
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
      if (tokDebug >= 5)
	*Log(theErrLog) << "[countSentences] buffer#" <<i 
			<< " word=[" << UnicodeToUTF8( tokens[i].us) 
			<<"] role=" << tokens[i].role 
			<< ", quotelevel="<< quotelevel << endl;
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;	
      if ((forceentirebuffer) && (tokens[i].role & TEMPENDOFSENTENCE) && (quotelevel == 0)) {
	//we thought we were in a quote, but we're not... No end quote was found and an end is forced now.
	//Change TEMPENDOFSENTENCE to ENDOFSENTENCE and make sure sentences match up sanely
	tokens[i].role ^= TEMPENDOFSENTENCE;
	tokens[i].role |= ENDOFSENTENCE;
	if (!(tokens[begin].role & BEGINOFSENTENCE)) {
	  tokens[begin].role |= BEGINOFSENTENCE;
	}
      }		
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	begin = i + 1;
	count++;
	if (tokDebug >= 5) 
	  *Log(theErrLog) << "[countSentences] SENTENCE #" << count << " found" << endl;
	if ((begin < size) && !(tokens[begin].role & BEGINOFSENTENCE)) {
	  tokens[begin].role |= BEGINOFSENTENCE;
	}
      }
      if ((forceentirebuffer) && (i == size - 1) && !(tokens[i].role & ENDOFSENTENCE))  {
	//last token of buffer
	count++;
	tokens[i].role |= ENDOFSENTENCE;
	if (tokDebug >= 5) 
	  *Log(theErrLog) << "[countSentences] SENTENCE #" << count << " *FORCIBLY* ended" << endl;	    
      }
    }
    return count;
  }
  
  int TokenizerClass::flushSentences(int sentences) {
    //Flush n sentences from the buffer, returns the number of tokens left
    short quotelevel = 0;
    const int size = tokens.size();
    if (sentences == 0) return size;
    int begin = 0;	
    for (int i = 0; i < size; i++) {
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;	
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) begin = i + 1;
    }
    if (begin == 0) {
      throw uLogicError("Unable to flush, not so many sentences in buffer");
    }
    if (begin == size) {
      tokens.clear();
      quotes.clearStack();
    } else {
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
  
  bool TokenizerClass::getSentence( int index, int& begin, int& end ) {
    int count = 0;
    const int size = tokens.size();
    short quotelevel = 0;
    begin = 0;
    for (int i = 0; i < size; i++) {
	if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
	if (tokens[i].role & BEGINQUOTE) quotelevel++;
        if (tokens[i].role & ENDQUOTE) quotelevel--;	
	if ((tokens[i].role & BEGINOFSENTENCE) && (quotelevel == 0)) {
	  begin = i;
	}
        if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	  if (count == index) {
	    end = i;
	    if (!(tokens[begin].role & BEGINOFSENTENCE)) //sanity check
	      tokens[begin].role |= BEGINOFSENTENCE;
	    return true;
	  }
	  count++;
	}	
    }  
    return false;
  }
  
  vector<Token*> TokenizerClass::getSentence( int selectsentence) { 
    //index starts at 0
    short quotelevel = 0;
    const int size = tokens.size();
    vector<Token*> sentence;
    int count = 0;
    for (int i = 0; i < size; i++) {
      if (count == selectsentence) {
	sentence.push_back(&tokens[i]);
      }
      if (tokens[i].role & NEWPARAGRAPH) quotelevel = 0;
      if (tokens[i].role & BEGINQUOTE) quotelevel++;
      if (tokens[i].role & ENDQUOTE) quotelevel--;
      if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	if (selectsentence == count) {
	  return sentence;
	} else {
	  count++;
	}
      }
    }
    return sentence;
  }

  string TokenizerClass::getSentenceString( unsigned int i, 
					    const bool firstoutput ) {
    int begin, end;
    if (!getSentence(i,begin,end)) {
	throw uRangeError( "No sentence exists with the specified index: " 
			   + toString( i ) );
    }
    
    //This only makes sense in non-verbose mode, force verbose=false
    stringstream TMPOUT;
    const bool t = verbose;
    verbose = false;
    outputTokens( TMPOUT, begin,end, firstoutput);
    verbose = t;
    return TMPOUT.str(); 
  }
  
  bool TokenizerClass::terminatesWithEOS( ) const {
    if ( tokens.size() < 1 )
      return false;
    else
      return (tokens[tokens.size() - 1].role & ENDOFSENTENCE);
  }

  bool TokenizerClass::resolveQuote(int endindex, const UnicodeString& open ) {
    //resolve a quote        
    int stackindex = -1;
    int beginindex = quotes.lookup( open, stackindex );
    

    if (beginindex >= 0) {
      if (tokDebug >= 2) *Log(theErrLog) << "[resolveQuote] Quote found, begin="<< beginindex << ", end="<< endindex << endl;

      if (beginindex > endindex) {
	throw uRangeError( "Begin index for quote is higher than end index!" );
      }    
    
      
      //We have a quote!
      tokens[beginindex].role |= BEGINQUOTE;
      tokens[endindex].role |= ENDQUOTE;
      
      /*if ((endindex - 1 > beginindex) && (tokens[endindex-1].role & ENDOFSENTENCE)) {
	//Set proper BOS markers
	tokens[beginindex+1].role |= BEGINOFSENTENCE;
      }*/
      
      /*if (tokens[endindex].role & BEGINOFSENTENCE) {
	//final quote can never be begin of sentence, undo:
	tokens[endindex].role ^= BEGINOFSENTENCE;
      }*/
      
      //resolve sentences within quote, all sentences must be full sentences:
      int beginsentence = beginindex + 1;
      for (int i = beginsentence; i < endindex; i++) {
	if (tokens[i].role & TEMPENDOFSENTENCE) {			    
	    tokens[i].role ^= TEMPENDOFSENTENCE;
	    tokens[i].role |= ENDOFSENTENCE;
	    tokens[beginsentence].role |= BEGINOFSENTENCE;
	    beginsentence = i + 1;
	}
	/*  
	if (tokens[i].role & BEGINOFSENTENCE) {
	  if (i - 1 > beginindex)
	    tokens[i-1].role |= ENDOFSENTENCE;
	}
	if (tokens[i].role & ENDOFSENTENCE) {
	  if (i + 1 < endindex)
	    tokens[i+1].role |= BEGINOFSENTENCE;
	}*/
       }
	    
	  
	
      
      
      //remove from stack (ok, granted, stack is a bit of a misnomer here)
      quotes.eraseAtPos( stackindex );
      return true;
    } else {
      return false;
    }    
  }

  bool checkEos( UChar c ){
    bool is_eos = false;
    UBlockCode s = ublock_getCode(c);
    //test for languages that distinguish case
    if ((s == UBLOCK_BASIC_LATIN) || (s == UBLOCK_GREEK) ||
	(s == UBLOCK_CYRILLIC) || (s == UBLOCK_GEORGIAN) ||
	(s == UBLOCK_ARMENIAN) || (s == UBLOCK_DESERET)) { 
      if ( u_isupper(c) || u_istitle(c) || u_ispunct(c) ) {
	//next 'word' starts with more punctuation or with uppercase
	is_eos = true;
      }
    } else {
      // just normal ASCII punctuation
      is_eos = true;
    }
    return is_eos;
  }

  void TokenizerClass::detectQuoteBounds( const int i, const UChar c ) {
    //Detect Quotation marks
    if ((c == '"') || ( UnicodeString(c) == "＂") ) {
      if (tokDebug > 1 )
	*Log(theErrLog) << "[detectQuoteBounds] Standard double-quote (ambiguous) found @i="<< i << endl;
      if (!resolveQuote(i,c)) {
	 if (tokDebug > 1 ) *Log(theErrLog) << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	quotes.push( i, c );
      }
    } else {
      UnicodeString close = quotes.lookupOpen( c );
      if ( !close.isEmpty() ){ // we have a opening quote
	if (tokDebug > 1 ) *Log(theErrLog) << "[detectQuoteBounds] Opening quote found @i="<< i << ", pushing to stack for resultion later..." << endl;	      
	quotes.push( i, c ); // remember it
      } else {
	UnicodeString open = quotes.lookupClose( c );	
	if ( !open.isEmpty() ) { // we have a closeing quote
	 if (tokDebug > 1 ) *Log(theErrLog) << "[detectQuoteBounds] Closing quote found @i="<< i << ", attempting to resolve..." << endl;	      
	  if (!resolveQuote(i, open )) // resolve the matching opening
	   if (tokDebug > 1 ) *Log(theErrLog) << "[detectQuoteBounds] Unable to resolve" << endl;	      
        //} else if (UnicodeString(c) == "―") {
	  //TODO: Implement
	}

      }
    }
  }
  
  void TokenizerClass::detectSentenceBounds( const int offset ){
    //find sentences
    const int size = tokens.size();
    for (int i = offset; i < size; i++) {
      if ((offset == 0) && (sentencesignal)) {
	tokens[i].role |= BEGINOFSENTENCE;
	sentencesignal = false;
      }
      if (tokDebug > 1 )
	*Log(theErrLog) << "[detectSentenceBounds] i="<< i << " word=[" 
			<< UnicodeToUTF8( tokens[i].us ) 
			<<"] role=" << tokens[i].role << endl;
      if ( tokens[i].type->startsWith( "PUNCTUATION") ) { //TODO: make comparison more efficient?
	UChar c = tokens[i].us[0]; 
	bool is_eos = false;
	if (c == '.') {	
	  if (i + 1 == size) {	//No next character? 
	    is_eos = true; //Newline after period
	  } else {
	    // check next token for eos
	    is_eos = checkEos(tokens[i+1].us[0] );
	  }
	} else { //no period
	  //Check for other EOS markers
	  if ( eosmarkers.indexOf( c ) >= 0 )
	    is_eos = true;
	}
	if (is_eos) {
	  
	  
	  if ((detectQuotes) && (!quotes.emptyStack())) {
	      
	      if ((tokDebug > 1 )) 
		*Log(theErrLog) << "[detectSentenceBounds] Preliminary EOS FOUND @i=" << i << endl;
	      
	      //if there are quotes on the stack, we set a temporary EOS marker, to be resolved later when full quote is found.
	      tokens[i].role |= TEMPENDOFSENTENCE;
	      //If previous token is also TEMPENDOFSENTENCE, it stops being so in favour of this one
	      if ((i > 0) && (tokens[i-1].role & TEMPENDOFSENTENCE))
		 tokens[i-1].role ^= TEMPENDOFSENTENCE;
	  } else { //No quotes on stack
	      sentencesignal = true;
	      if ((tokDebug > 1 )) 
		*Log(theErrLog) << "[detectSentenceBounds] EOS FOUND @i=" << i << endl;
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

	} else if (detectQuotes) {
	  //check for other bounds
	  detectQuoteBounds(i,c);
	}	

	
      }
    }
  }    
  
  TokenizerClass::~TokenizerClass(){
    for ( unsigned int i = 0; i < rules.size(); i++) {
      delete rules[i];
    }
  }

  void TokenizerClass::passthruLine( const string& input, bool& bos ) {
    if (tokDebug) *Log(theErrLog) << "[passthruLine] input: line=[" << input << "]" << endl;
    bool alpha = false, num = false, punct = false;
    UnicodeString word;
    for ( size_t i=0; i < input.length(); ++i ) {
      UChar c = input[i];    
      
      if ( u_isspace(c)) {
	if ( word == explicit_eos_marker ) {
	  if (!tokens.empty()) 
	    tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
	  bos = true;
	} else {        
	  const UnicodeString *type;
	  if (alpha && !num && !punct) {
	    type = &type_word;
	  } else if (num && !alpha && !punct) {
	    type = &type_number;                
	  } else if (punct && !alpha && !num) {
	    type = &type_punctuation;                    
	  } else {
	    type = &type_unknown;                
	  }
	  if (bos) {
	    tokens.push_back( Token( type, word , BEGINOFSENTENCE ) );
	    bos = false;
	  } else {
	    tokens.push_back( Token( type, word ) );
	  }
	  alpha = false;
	  num = false;
	  punct = false;
          word = "";
	}
      } else {
	if ( u_isalpha(c)) {
	  alpha = true;
	} else if (u_ispunct(c)) {
	  punct = true;
	} else if (u_isdigit(c)) {
	  num = true;
	}            
	word += c;
      }    
    }
    if (word != "") {
	  const UnicodeString *type;
	  if (alpha && !num && !punct) {
	    type = &type_word;
	  } else if (num && !alpha && !punct) {
	    type = &type_number;                
	  } else if (punct && !alpha && !num) {
	    type = &type_punctuation;                    
	  } else {
	    type = &type_unknown;                
	  }
    	  if (bos) {
	    tokens.push_back( Token( type, word , BEGINOFSENTENCE ) );
	    bos = false;
	  } else {
	    tokens.push_back( Token( type, word ) );
	  }
    }
  }

  // string wrapper
  int TokenizerClass::tokenizeLine( const string& s ){
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
    else
      return tokenizeLine( uinputstring );
  }  
  
  int TokenizerClass::tokenizeLine( const UnicodeString& originput ){ 
    if (tokDebug) 
      *Log(theErrLog) << "[tokenizeLine] input: line=[" 
		      << UnicodeToUTF8( originput ) << "]" << endl;
    UnicodeString input = normalizer.normalize( originput );
    if ( doFilter ){
      input = filter.filter( input );
    }
    if (tokDebug) 
      *Log(theErrLog) << "[tokenizeLine] filtered input: line=[" 
		      << UnicodeToUTF8( input ) << "]" << endl;
    
    const int begintokencount = tokens.size();    
    if (tokDebug) *Log(theErrLog) << "[tokenizeLine] Tokens still in buffer: " << begintokencount << endl;
    
    if ( !input.isBogus() ){ //only tokenize valid input
      bool tokenizeword = false;
      bool possible_abbreviation = false;
      bool reset = false;
      //iterate over all characters
      UnicodeString word;
      for ( int i=0; i < input.length(); ++i ) {
	UChar c = input[i];
	if (reset) { //reset values for new word
	  reset = false;
	  if (!u_isspace(c)) word = c; else word = "";
	  tokenizeword = false;
	} else {
	  if (!u_isspace(c)) word += c;
	}
	if ( u_isspace(c) || i == input.length()-1 ){
	  if (tokDebug)
	    *Log(theErrLog) << "[tokenizeLine] space detected, word=[" 
			    << UnicodeToUTF8( word ) << "]" << endl;
	  if ( i == input.length()-1 ) {
	    if ( u_ispunct(c) || u_isdigit(c)) tokenizeword = true; 
	  } else { // isspace
	    //word.remove(word.length()-1);
	  }
	  int expliciteosfound = -1;
	  if ( word.length() >= explicit_eos_marker.length() ) {
	    expliciteosfound = word.lastIndexOf(explicit_eos_marker);	    
	    if (expliciteosfound != -1) { //( word == explicit_eos_marker ) {
	      if (tokDebug >= 2) 
		*Log(theErrLog) << "[tokenizeLine] Found explicit EOS marker @"<<expliciteosfound << endl;
	      if (expliciteosfound > 0) {		    		    
		UnicodeString realword;		    
		word.extract(0,explicit_eos_marker.length(),realword);
		if (tokDebug >= 2)
		  *Log(theErrLog) << "[tokenizeLine] Prefix before EOS: "
				  << UnicodeToUTF8( realword ) << endl;
		tokenizeWord( realword, false );
	      }
	      if (expliciteosfound + explicit_eos_marker.length() < word.length())  {
		UnicodeString realword;		    
		word.extract(expliciteosfound+explicit_eos_marker.length(),word.length() - expliciteosfound - explicit_eos_marker.length(),realword);
		if (tokDebug >= 2) 
		  *Log(theErrLog) << "[tokenizeLine] Prefix after EOS: "
				  << UnicodeToUTF8( realword ) << endl;
		tokenizeWord( realword, true );
	      }
	      if (!tokens.empty()) {
		if (tokDebug >= 2) 
		  *Log(theErrLog) << "[tokenizeLine] Assigned EOS\n";
		tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
	      }			
	    }
	  }	
	  if ((word.length() > 0) && (expliciteosfound == -1)) {	    
	    if (!tokenizeword) {	      
		//single character or nothing tokenisable found, so no need to tokenize anything
		if (tokDebug >= 2)
		  *Log(theErrLog) << "[tokenizeLine] Word ok, no need for further tokenisation for: ["
				  << UnicodeToUTF8( word ) << "]" << endl;;
		tokens.push_back( Token( &type_word, word ) );
	    } else {
	      if (tokDebug >= 2)
		*Log(theErrLog) << "[tokenizeLine] Further tokenisation necessary for: [" 
				<< UnicodeToUTF8( word ) << "]" << endl;
	      tokenizeWord( word, true );            
	    } 
	  }
	  //reset values for new word
	  reset = true;        
	} else if ( u_ispunct(c) || u_isdigit(c)) {
	  if (tokDebug) 
	    *Log(theErrLog) << "[tokenizeLine] punctuation or digit detected, word=[" 
			    << UnicodeToUTF8( word ) << "]" << endl;
	  
	  //there is punctuation or digits in this word, mark to run through tokeniser
	  tokenizeword = true; 
	}
	if (c == '.') {
	  possible_abbreviation = true;
	} else {
	  possible_abbreviation = false;
	}
      }        	
    } else {
      //ELSE print error message
      *theErrLog << "ERROR: Invalid UTF-8 in line!" << endl;
    }
    int numNewTokens = tokens.size() - begintokencount;
    if (paragraphsignal) {
 	tokens[begintokencount].role |= NEWPARAGRAPH | BEGINOFSENTENCE;
	paragraphsignal = false;
    }
    if ( detectBounds )
      detectSentenceBounds( begintokencount );  //find sentence boundaries
    return numNewTokens;
  }
  
  bool Rule::matchAll( const UnicodeString& line,
		       UnicodeString& pre,
		       UnicodeString& post,
		       vector<UnicodeString>& matches ){
    matches.clear();
    pre = "";
    post = "";
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
    if ( tokDebug > 2 )
      *Log(theErrLog) << "   [tokenizeWord] Input: (" << input.length() << ") "
		      << "word=[" << UnicodeToUTF8( input ) << "]" << endl;
    if ( input == explicit_eos_marker ) {
      if (tokDebug >= 2)
	*Log(theErrLog) << "   [tokenizeWord] Found explicit EOS marker" << endl;
      if (!tokens.empty()) {
	if (tokDebug >= 2) 
	  *Log(theErrLog) << "   [tokenizeWord] Assigned EOS\n";
	tokens[tokens.size() - 1].role |= ENDOFSENTENCE;
      } else {
	*Log(theErrLog) << "[WARNING] Found explicit EOS marker by itself, this will have no effect!\n"; 
      }
      return;
    }
    
    if ( input.length() == 1) {
      //single character, no need to process all rules, do some simpler (faster) detection
      UChar c = input[0];
      const UnicodeString *type;
      
      if ( u_ispunct(c)) {
	if (  u_charType( c ) == U_CURRENCY_SYMBOL ) {
	  type = &type_currency;
	} else {
	  type = &type_punctuation;
	}
      } else if ( u_isalpha(c)) {
	type = &type_word;
      } else if ( u_isdigit(c)) {
	type = &type_number;
      } else if ( u_isspace(c)) {
	return;
      } else {
	if (  u_charType( c ) == U_CURRENCY_SYMBOL ) {
	  type = &type_currency;
	} else {
	  type = &type_unknown;
	}
      } 
      tokens.push_back( Token( type, input, space ? NOROLE : NOSPACE ) ); 
    } else {
      for ( unsigned int i = 0; i < rules.size(); i++) {
	if ( tokDebug >= 4)
	  *Log(theErrLog) << "\tTESTING " << UnicodeToUTF8( rules[i]->id )
			  << endl;
	//Find first matching rule
	UnicodeString pre, post;
	vector<UnicodeString> matches;
	if ( rules[i]->matchAll( input, pre, post, matches ) ){
	  if ( tokDebug >= 4 )
	    *Log(theErrLog) << "\tMATCH: " << UnicodeToUTF8( rules[i]->id )
			    << endl;    
	  if ( pre.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      *Log(theErrLog) << "\tTOKEN pre-context (" << pre.length() 
			      << "): [" << UnicodeToUTF8( pre ) << "]" << endl;
	    }
	    tokenizeWord( pre, false ); //pre-context, no space after
	  }
	  if ( matches.size() > 0 ){
	    int max = matches.size();
	    if ( tokDebug >= 4 )
	      *Log(theErrLog) << "\tTOKEN match #=" << matches.size() << endl;
	    for ( int m=0; m < max; ++m ){
	      if ( tokDebug >= 4 )
		*Log(theErrLog) << "\tTOKEN match[" << m << "] = " 
				<< UnicodeToUTF8( matches[m] )<< endl; 
	      tokens.push_back( Token( &rules[i]->id, matches[m], space ? NOROLE : NOSPACE ) );
	    }
	  }
	  else if ( tokDebug >=4 )
	    *Log(theErrLog) << "\tthere's no match" << endl;
	  if ( post.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      *Log(theErrLog) << "\tTOKEN post-context (" << post.length() 
			      << "): [" << UnicodeToUTF8(post) << "]" << endl;
	    }
	    tokenizeWord( post, space ? NOROLE : NOSPACE );
	  }
	  break;
	}
      }
    }
  }

  bool TokenizerClass::readrules( const string& fname) {
    if ( tokDebug > 0 )
      *theErrLog << "%include " << fname << endl;
    ifstream f(fname.c_str());
    if ( !f ){
      return false;
    }    
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 )
	    *theErrLog << "include line = " << rawline << endl;
	  const int splitpoint = line.indexOf("=");
	  if ( splitpoint < 0 ){
	    throw uConfigError( "invalid RULES entry: " + line );
	  }
	  UnicodeString id = UnicodeString( line, 0,splitpoint);
	  UnicodeString pattern = UnicodeString( line, splitpoint+1);
	  rules.push_back( new Rule( id, pattern) );
	}
      }
    }    
    return true;
  }

  bool TokenizerClass::readfilters( const string& fname) {
    if ( tokDebug > 0 )
      *theErrLog << "%include " << fname << endl;
    ifstream f(fname.c_str());
    if ( !f ){
      return false;
    }    
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 )
	    *theErrLog << "include line = " << rawline << endl;
	  
	  UnicodeString open = "";
	  UnicodeString close = "";
	  int splitpoint = line.indexOf(" ");
	  if ( splitpoint == -1 )
	    splitpoint = line.indexOf("\t");
	  if ( splitpoint == -1 ){
	    open = line;
	  }
	  else {
	    open = UnicodeString( line, 0,splitpoint);
	    close = UnicodeString( line, splitpoint+1);
	  }
	  open = open.trim().unescape();
	  close = close.trim().unescape();
	  if ( open.length() != 1 ){
	    throw uConfigError( "invalid FILTER entry: " + line );
	  }
	  else {
	    filter.add( open[0], close );
	  }
	}
      }
    }    
    return true;
  }
  
  bool TokenizerClass::readquotes( const string& fname) {
    if ( tokDebug > 0 )
      *theErrLog << "%include " << fname << endl;
    ifstream f(fname.c_str());
    if ( !f ){
      return false;
    }    
    else {
      string rawline;
      while ( getline(f,rawline) ){
	UnicodeString line = UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 )
	    *theErrLog << "include line = " << rawline << endl;
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
  
  ConfigMode getMode( const UnicodeString& line ) {
    ConfigMode mode = NONE;
    if (line == "[RULES]") {
      mode = RULES;
    } else if (line == "[RULE-ORDER]") {
      mode = RULEORDER;
    } else if (line == "[ABBREVIATIONS]") {
      mode = ABBREVIATIONS;
    } else if (line == "[ATTACHEDPREFIXES]") {
      mode = ATTACHEDPREFIXES;
    } else if (line == "[ATTACHEDSUFFIXES]") {
      mode = ATTACHEDSUFFIXES;
    } else if (line == "[PREFIXES]") {
      mode = PREFIXES;
    } else if (line == "[SUFFIXES]") {
      mode = SUFFIXES;
    } else if (line == "[TOKENS]") {
      mode = TOKENS;
    } else if (line == "[UNITS]") {
      mode = UNITS;
    } else if (line == "[ORDINALS]") {
      mode = ORDINALS;
    } else if (line == "[EOSMARKERS]") {
      mode = EOSMARKERS;
    } else if (line == "[QUOTES]") {
      mode = QUOTES;
    } else if (line == "[FILTER]") {
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

  void sortRules( vector<Rule *>& rules, vector<UnicodeString>& sort ){
    // *Log(theErrLog) << "rules voor sort : " << endl;
    // for ( size_t i=0; i < rules.size(); ++i ){
    //   *Log(theErrLog) << "rule " << i << " " << *rules[i] << endl;
    // }
    if ( !sort.empty() ){
      vector<Rule *> result;
      for ( size_t i=0; i < sort.size(); ++i ){
	vector<Rule*>::iterator it = rules.begin();
	bool found = false;
	while ( it != rules.end() && !found ){
	  if ( (*it)->id == sort[i] ){
	    result.push_back( *it );
	    it = rules.erase( it );
	    found = true;
	  }
	  ++it;
	}
	if ( !found ){
	  cerr << "RULE-ORDER specified for undefined RULE '" 
	       << UnicodeToUTF8( sort[i] ) << "'" << endl;
	}
      }
      vector<Rule*>::iterator it = rules.begin();
      while ( it != rules.end() ){
	cerr << "NU RULE-ORDER specified for RULE '" 
	     << UnicodeToUTF8((*it)->id) << "'" << endl;
	result.push_back( *it );
	++it;
      }
      rules = result;
    }
    // *Log(theErrLog) << "rules NA sort : " << endl;
    // for ( size_t i=0; i < rules.size(); ++i ){
    //   *Log(theErrLog) << "rule " << i << " " << *rules[i] << endl;
    // }
  }
  
  bool TokenizerClass::readsettings( const string& dir, const string& fname) {

    ConfigMode mode = NONE;
    
    UnicodeString abbrev_pattern = "";
    UnicodeString prefix_pattern = "";
    UnicodeString token_pattern = "";
    UnicodeString suffix_pattern = "";
    UnicodeString withprefix_pattern = "";
    UnicodeString withsuffix_pattern = "";
    UnicodeString unit_pattern = "";
    UnicodeString ordinal_pattern = "";

    vector<UnicodeString> rules_order;

    string conffile;
    if ( fname.find("/") != string::npos ){
      // ignore dir when fname seems a relative or absolute path
      // maybe a bit rude?
      conffile = fname;
    }
    else {
      conffile = dir + fname;
    }
    ifstream f(conffile.c_str());
    if ( !f ){
      return false;
    }    
    else {
      string rawline;
      while ( getline(f,rawline) ){
	if ( rawline.find( "%include" ) != string::npos ){
	  switch ( mode ){
	  case RULES: {
	    string file = rawline.substr( 9 );
	    file = dir + file + ".rule";
	    if ( !readrules( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case FILTER:{
	    string file = rawline.substr( 9 );
	    file = dir + file + ".filter";
	    if ( !readfilters( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  case QUOTES:{
	    string file = rawline.substr( 9 );
	    file = dir + file + ".quote";
	    if ( !readquotes( file ) )
	      throw uConfigError( "%include '" + file + "' failed" );
	  }
	    break;
	  default:
	    throw uConfigError( string("%include not implemented for this section" ) );
	  }
	  continue;
	}
	
	UnicodeString line = UTF8ToUnicode(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if (line[0] == '[') {
	    mode = getMode( line );
	  }
	  else {
	    switch( mode ){
	    case RULES: {
	      const int splitpoint = line.indexOf("=");
	      if ( splitpoint < 0 ){
		throw uConfigError( "invalid RULES entry: " + line );
	      }
	      UnicodeString id = UnicodeString( line, 0,splitpoint);
	      UnicodeString pattern = UnicodeString( line, splitpoint+1);
	      rules.push_back( new Rule( id, pattern) );
	    }
	      break;
	    case RULEORDER:
	      addOrder( rules_order, line );
	      break;
	    case ABBREVIATIONS:
	      if (!abbrev_pattern.isEmpty()) abbrev_pattern += '|';
	      abbrev_pattern += line;
	      break;
	    case ATTACHEDPREFIXES: 
	      if (!withprefix_pattern.isEmpty()) withprefix_pattern += '|';
	      withprefix_pattern += line;
	      break;
	    case ATTACHEDSUFFIXES:
	      if (!withsuffix_pattern.isEmpty()) withsuffix_pattern += '|';
	      withsuffix_pattern += line;
	      break;
	    case PREFIXES:
	      if (!prefix_pattern.isEmpty()) prefix_pattern += '|';
	      prefix_pattern += line;
	      break;
	    case SUFFIXES:
	      if (!suffix_pattern.isEmpty()) suffix_pattern += '|';
	      suffix_pattern += line;
	      break;
	    case TOKENS:
	      if (!token_pattern.isEmpty()) token_pattern += '|';
	      token_pattern += line;
	      break;
	    case UNITS:
	      if (!unit_pattern.isEmpty()) unit_pattern += '|';
	      unit_pattern += line;
	      break;
	    case ORDINALS:
	      if (!ordinal_pattern.isEmpty()) ordinal_pattern += '|';
	      ordinal_pattern += line;
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
	    case FILTER: {
	      UnicodeString open = "";
	      UnicodeString close = "";
	      int splitpoint = line.indexOf(" ");
	      if ( splitpoint == -1 )
		splitpoint = line.indexOf("\t");
	      if ( splitpoint == -1 ){
		open = line;
	      }
	      else {
		open = UnicodeString( line, 0,splitpoint);
		close = UnicodeString( line, splitpoint+1);
	      }
	      open = open.trim().unescape();
	      close = close.trim().unescape();
	      if ( open.length() != 1 ){
		throw uConfigError( "invalid FILTER entry: " + line );
	      }
	      else {
		filter.add( open[0], close );
	      }
	    }
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
      eosmarkers = "!?";
    }
    if ( quotes.empty() ){
      quotes.add( '"', '"' );
      quotes.add( "‘", "’" );
      quotes.add( "“„‟", "”" );
    }

    // Create Rules for every pattern that is set
    
    //TODO: Make order dynamically configurable?

    if (!ordinal_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("NUMBER-ORDINAL", "(?i)\\p{N}+-?(?:" + ordinal_pattern + ")(?:\\Z|\\P{L})")); 
    }
    /*if (!unit_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("UNIT", "(?i)(?:\\a|\\P{L})(" + unit_pattern + ")(?:\\z|\\P{L})")); 
      }*/
    if (!abbrev_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("ABBREVIATION-KNOWN",  "(?i)(?:\\p{P}*)?(?:\\A|[^\\p{L}\\.])((?:" + abbrev_pattern + ")\\.)(?:\\Z|\\P{L})")); //case insensitive
    }
    if (!token_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("WORD-TOKEN", "(?i)^(" + token_pattern + ")(?:\\p{P}*)?$")); // (?:\\Z|\\P{L})")); 
    }    
    if (!withprefix_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("WORD-WITHPREFIX", "(?i)(?:\\A|[^\\p{L}\\.])(?:" + withprefix_pattern + ")\\p{L}+")); 
    }
    if (!withsuffix_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("WORD-WITHSUFFIX", "(?i)(\\p{L}+(?:" + withsuffix_pattern + "))(?:\\Z|\\P{L})")); 
    }
    if (!prefix_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("PREFIX", "(?i)(?:\\A|[^\\p{L}\\.])(" + prefix_pattern + ")(\\p{L}+)")); 
    }
    if (!suffix_pattern.isEmpty()){
      rules.insert(rules.begin(), new Rule("SUFFIX", "(?i)(\\p{L}+)(" + suffix_pattern + ")(?:\\Z|\\P{L})")); 
    }
    //rules.insert(rules.begin(), new Rule("EOSMARKER", "(?:.*)?(" + *explicit_eos_marker + ")(?:.*)?")); 

    sortRules( rules, rules_order );
    return true;
  }
  
  bool TokenizerClass::init( const string& cdir, const string& fname ){
    *Log(theErrLog) << "Initiating tokeniser...\n";
    if (!readsettings( cdir, fname)) {
      throw uConfigError( "Cannot read Tokeniser settingsfile "+ cdir + fname );
      return false;
    }
    settingsfilename = fname;
    if ( tokDebug ){
      *Log(theErrLog) << "effective rules: " << endl;
      for ( size_t i=0; i < rules.size(); ++i ){
	*Log(theErrLog) << "rule " << i << " " << *rules[i] << endl;
      }
      *Log(theErrLog) << "EOS markers: " << UnicodeToUTF8( eosmarkers ) << endl;
      *Log(theErrLog) << "Quotations: " << quotes << endl;
      *Log(theErrLog) << "Filter: " << filter << endl;
    }
    return true;
  }
  
}//namespace
