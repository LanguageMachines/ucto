/*
  $Id$
  $URL$
  Copyright (c) 2006 - 2011
  Tilburg University

  This file is part of Ucto.

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

#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include "config.h"
#include "ucto/unicode.h"

namespace Tokenizer {

  std::string Version();
  std::string VersionName();

  //enum RuleTrigger { PUNCTUATION, PERIOD, NUMBER }; //TODO: implement
    
  enum TokenRole {
    NOROLE                      = 0,
    NOSPACE                     = 1,
    BEGINOFSENTENCE             = 2, 
    ENDOFSENTENCE               = 4,
    NEWPARAGRAPH                = 8,    
    BEGINQUOTE                  = 16, 
    ENDQUOTE                    = 32, 
    TEMPENDOFSENTENCE           = 64,
    LISTITEM                    = 128, //reserved for future use
    TITLE                       = 256 //reserved for future use
  };

  std::ostream& operator<<( std::ostream&, const TokenRole& );

  inline TokenRole operator|( TokenRole T1, TokenRole T2 ){
    return (TokenRole)( (int)T1|(int)T2 );
  }

  inline TokenRole& operator|= ( TokenRole& T1, TokenRole T2 ){
    T1 = (T1 | T2);
    return T1;
  }
  
  inline TokenRole operator^( TokenRole T1, TokenRole T2 ){
    return (TokenRole)( (int)T1^(int)T2 );
  }

  inline TokenRole& operator^= ( TokenRole& T1, TokenRole T2 ){
    T1 = (T1 ^ T2);
    return T1;
  }
  
  class Token {
    friend std::ostream& operator<< (std::ostream&, const Token& );
  public:
    const UnicodeString *type;
    UnicodeString us;
    TokenRole role; 
    Token( const UnicodeString *,
	   const UnicodeString& s,
	   TokenRole role = NOROLE ); 
  };

  class UnicodeRegexMatcher;

  class Rule {
    friend std::ostream& operator<< (std::ostream&, const Rule& );
  public:
  Rule(): regexp(0){
    };
    Rule( const UnicodeString& id, const UnicodeString& pattern);
    ~Rule();
    UnicodeString id;
    UnicodeString pattern;
    UnicodeRegexMatcher *regexp;
    bool matchAll( const UnicodeString&,
		   UnicodeString&,
		   UnicodeString&,
		   std::vector<UnicodeString>& );
    
  };

  class Quoting {
    friend std::ostream& operator<<( std::ostream&, const Quoting& );
    struct QuotePair {
      UnicodeString openQuote;
      UnicodeString closeQuote;
    };
  public:
    void add( const UnicodeString&, const UnicodeString& );
    UnicodeString lookupOpen( const UnicodeString &) const;
    UnicodeString lookupClose( const UnicodeString & ) const;
    bool empty() const { return quotes.empty(); };
    bool emptyStack() const { return quotestack.empty(); };
    void clearStack() { quoteindexstack.clear(); quotestack.clear(); };
    int lookup( const UnicodeString&, int& );
    void eraseAtPos( int pos ) {
      quotestack.erase( quotestack.begin()+pos );
      quoteindexstack.erase( quoteindexstack.begin()+pos );
    }
    void flushStack( int ); //renamed from eraseBeforeIndex
    void push( int i, UChar c ){
      quoteindexstack.push_back(i);
      quotestack.push_back(c);
    }
  private:
    std::vector<QuotePair> quotes;
    std::vector<int> quoteindexstack;
    std::vector<UChar> quotestack;
  };
  
  class TokenizerClass{
  public:
    TokenizerClass();
    ~TokenizerClass();
    bool init( const std::string&, const std::string& );
    void setErrorLog( std::ostream *os ) { theErrLog = os; };
    
    //Tokenize from input stream to output stream
    void tokenize( std::istream&, std::ostream& );
    void tokenize( std::istream* in, std::ostream* out){
      // for backward compatability
      return tokenize( *in, *out );};
    
    //Tokenize a line (a line is NOT a sentence, but an arbitrary line of input)
    int tokenizeLine( const UnicodeString& );
    int tokenizeLine( const std::string& );
    
    void passthruLine( const std::string&, bool& );    
    
    bool empty() const { return tokens.empty(); };
        
    //Processes tokens and initialises the sentence buffer. Returns the amount of sentences found
    int countSentences(bool forceentirebuffer = false); //count the number of sentences (only after detectSentenceBounds) (does some extra validation as well)
    int flushSentences(const int); //Flush n sentences from buffer (does some extra validation as well)
    
    
    
    //Get the begin and end index of the sentence with the specified index
    bool getSentence( int, int& begin, int& end );
    
    //Get the sentence with the specified index as a vector of tokens
    std::vector<Token*> getSentence( int );    
    
    //Get the sentence with the specified index as a string (UTF-8 encoded)
    std::string getSentenceString( unsigned int, const bool = false);
    
    //Get all sentences as a vector of strings (UTF-8 encoded)
    std::vector<std::string> getSentences() const;
            
    void detectSentenceBounds( const int offset = 0 );
    void detectQuoteBounds( const int, const UChar);
    
    //Does the token buffer terminate with a proper EOS marker?
    bool terminatesWithEOS( ) const;
    
    //Enable verbose mode
    bool setVerbose( bool b=true ) { bool t = verbose; verbose = b; return t; };
    bool getVerbose() { return verbose; }
    
    //set debug value
    int setDebug( int d ) { bool dd = tokDebug; tokDebug = d; return dd; };
    int getDebug() { return tokDebug; }
    
    //Enable conversion of all output to lowercase
    bool setLowercase( bool b=true ) { bool t = lowercase; lowercase = b; if (b) uppercase = false; return t; };
    bool getLowercase() { return lowercase; }

    //Enable passtru mode
    bool setPassThru( bool b=true ) { bool t = passthru; passthru = b; return t; };
    bool getPassThru() { return passthru; }
    
    //Enable conversion of all output to uppercase
    bool setUppercase( bool b=true ) { bool t = uppercase; uppercase = b; if (b) lowercase = false; return t; };
    bool getUppercase() { return uppercase; }

    //Enable sentence-bound detection
    bool setSentenceDetection( bool b=true ) { bool t = detectBounds; detectBounds = b; return t; }
    bool getSentenceDetection() { return detectBounds; }
    
    //Enable paragraph detection
    bool setParagraphDetection( bool b=true ) { bool t = detectPar; detectPar = b; return t; }
    bool getParagraphDetection() { return detectPar; }
    
    //Enable quote detection
    bool setQuoteDetection( bool b=true ) { bool t = detectQuotes; detectQuotes = b; return t; }
    bool getQuoteDetection() const { return detectQuotes; }

    //Enable filtering
    bool setFiltering( bool b=true ) { bool t = doFilter; doFilter = b; return t; }
    bool getFiltering() const { return doFilter; };

    // set normalization mode
    std::string setNormalization( const std::string& s ) {
      return normalizer.setMode( s );
    }
    std::string getNormalization() const { return normalizer.getMode(); };

    // set input encoding
    std::string setInputEncoding( const std::string& );
    std::string getInputEncoding() const { return inputEncoding; };
    
    // set eos marker
    std::string setEosMarker( const std::string& s = "<utt>") { std::string t = eosmark; eosmark = s; return t; };
    std::string getEosMarker( ) { return eosmark; }

    bool setSentencePerLineOutput( bool b=true ) { bool t = sentenceperlineoutput; sentenceperlineoutput = b; return t; };
    bool getSentencePerLineOutput() { return sentenceperlineoutput; }
    
    bool setSentencePerLineInput( bool b=true ) { bool t = sentenceperlineinput; sentenceperlineinput = b; return t; };
    bool getSentencePerLineInput() { return sentenceperlineinput; }    
    
    std::string getDocID() { return docid; }
    bool getXMLOutput() { return xmlout; }
    
    
    bool setXMLOutput(bool b, std::string id) { bool t = xmlout; docid = id; xmlout = b; return t; }
    
    //Signal the tokeniser that a paragraph is detected
    void signalParagraph( bool b=true ) { paragraphsignal = b; };
    
    
    void outputXMLHeader( std::ostream& );
    void outputXMLFooter( std::ostream&, bool);
    //bool outputSentenceXML(std::ostream*, unsigned int);
    
    void outputTokens( std::ostream&, const size_t, const size_t, const bool = false) const;
    void outputTokensXML( std::ostream&, const size_t, const size_t, bool&);
    
  private:
    void tokenizeWord( const UnicodeString&, bool);
    
    //Turn buffered tokens into a UnicodeString contai, also outputs types and roles in verbose mode
    
    bool resolveQuote( int, const UnicodeString& );
    bool detectEos( UChar );

    bool readsettings( const std::string&, const std::string& );
    bool readrules( const std::string& );
    bool readfilters( const std::string& );
    bool readquotes( const std::string& );
    
    //Don't use this in normal processing, use flushSentences instead
    void clear() { tokens.clear(); quotes.clearStack(); };

    Quoting quotes;
    UnicodeFilter filter;
    UnicodeNormalizer normalizer;    
    UnicodeString eosmarkers;
    std::string inputEncoding;

    std::string eosmark;
    std::vector<Token> tokens;
        
    std::vector<Rule *> rules;
    std::ostream *theErrLog;

    
    //debug flag
    int tokDebug;

    //verbose tokenisation mode
    bool verbose;
    
    //detect sentence bounds?
    bool detectBounds;
    
    //detect quotes?
    bool detectQuotes;
    
    //filter special characters (default on)?
    bool doFilter;
    
    //detect paragraphs?
    bool detectPar;
    
    //has a paragraph been signaled?
    bool paragraphsignal;
    //has a sentence been signaled?
    bool sentencesignal;
    
    //one sentence per line output
    bool sentenceperlineoutput;
    bool sentenceperlineinput;
    

    bool firstoutput;
    bool lowercase;
    bool uppercase;
    bool xmlout;  
    bool passthru;

    std::string settingsfilename;
    
    //Counters for xml output
    int count_p;
    int count_s;
    int count_w;    
    
    std::string docid; //document ID (UTF-8), necessary for XML output  
  };

  template< typename T >
    T stringTo( const std::string& str ) {
    T result;
    std::stringstream dummy ( str );
    if ( !( dummy >> result ) ) {
      throw( std::runtime_error( "conversion from '" + str + "' failed" ) );
    }
    return result;
  }
  
  template< typename T >
    std::string toString( const T val ) {    
    std::stringstream dummy;
    if ( !( dummy << val ) ) {
      throw( std::runtime_error( "conversion failed" ) );
    }
    return dummy.str();
  }
  
}
#endif
