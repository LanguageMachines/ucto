/*
  $Id$
  $URL$
  Copyright (c) 2006 - 2014
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

#ifndef UCTO_TOKENIZE_H
#define UCTO_TOKENIZE_H

#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include "ucto/unicode.h"
#include "ticcutils/LogStream.h"
#include "libfolia/document.h"

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

    std::string texttostring();
    std::string typetostring();
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
    void push( int i, UChar32 c ){
      quoteindexstack.push_back(i);
      quotestack.push_back(c);
    }
  private:
    std::vector<QuotePair> quotes;
    std::vector<int> quoteindexstack;
    std::vector<UChar32> quotestack;
  };

  class TokenizerClass{
  protected:
    int linenum;
  public:
    TokenizerClass();
    ~TokenizerClass();
    bool init( const std::string& );
    bool reset();
    void setErrorLog( TiCC::LogStream *os );

    // Tokenize from input stream to FoLiA document
    folia::Document tokenize( std::istream& );
    //
    // Tokenize a folia document
    bool tokenize(folia::Document& );

    //Tokenize from input stream to a vecto of Tokens
    std::vector<Token> tokenizeStream( std::istream&, bool allatonce=true );

    //Tokenize from input file to output file (support xmlin + xmlout)
    void tokenize( const std::string&, const std::string& );
    //Tokenize from input stream to output stream
    void tokenize( std::istream&, std::ostream& );
    void tokenize( std::istream* in, std::ostream* out){
      // for backward compatability
      return tokenize( *in, *out );};

    // Tokenize a line (a line is NOT a sentence, but an arbitrary string
    //                  of characters, inclusive EOS markers, Newlines etc.)
    int tokenizeLine( const UnicodeString& ); // Unicode chars
    int tokenizeLine( const std::string& );   // UTF8 chars

    void passthruLine( const std::string&, bool& );

    //Processes tokens and initialises the sentence buffer. Returns the amount of sentences found
    int countSentences(bool forceentirebuffer = false); //count the number of sentences (only after detectSentenceBounds) (does some extra validation as well)
    int flushSentences(const int); //Flush n sentences from buffer (does some extra validation as well)

    //Get the sentence with the specified index as a string (UTF-8 encoded)
    std::string getSentenceString( unsigned int );

    //return the sentence with the specified index in a Token vector;
    std::vector<Token> getSentence( int );

    //Get all sentences as a vector of strings (UTF-8 encoded)
    std::vector<std::string> getSentences();

    //Enable verbose mode
    bool setVerbose( bool b=true ) { bool t = verbose; verbose = b; return t; };
    bool getVerbose() const { return verbose; }

    //set debug value
    int setDebug( int d ) { bool dd = tokDebug; tokDebug = d; return dd; };
    int getDebug() const { return tokDebug; }

    //Enable conversion of all output to lowercase
    bool setLowercase( bool b=true ) { bool t = lowercase; lowercase = b; if (b) uppercase = false; return t; };
    bool getLowercase() const { return lowercase; }

    //Enable passtru mode
    bool setPassThru( bool b=true ) { bool t = passthru; passthru = b; return t; };
    bool getPassThru() const { return passthru; }

    //Enable conversion of all output to uppercase
    bool setUppercase( bool b=true ) { bool t = uppercase; uppercase = b; if (b) lowercase = false; return t; };
    bool getUppercase() const { return uppercase; }

    //Enable sentence-bound detection
    bool setSentenceDetection( bool b=true ) { bool t = detectBounds; detectBounds = b; return t; }
    bool getSentenceDetection() const { return detectBounds; }

    //Enable paragraph detection
    bool setParagraphDetection( bool b=true ) { bool t = detectPar; detectPar = b; return t; }
    bool getParagraphDetection() const { return detectPar; }

    //Enable quote detection
    bool setQuoteDetection( bool b=true ) { bool t = detectQuotes; detectQuotes = b; return t; }
    bool getQuoteDetection() const { return detectQuotes; }

    //Enable filtering
    bool setFiltering( bool b=true ) {
      bool t = doFilter; doFilter = b; return t;
    }
    bool getFiltering() const { return doFilter; };

    bool setPunctFilter( bool b=true ) {
      bool t = doPunctFilter; doPunctFilter = b; return t;
    }
    bool getPunctFilter() const { return doPunctFilter; };

    // set normalization mode
    std::string setNormalization( const std::string& s ) {
      return normalizer.setMode( s );
    }
    std::string getNormalization() const { return normalizer.getMode(); };

    // set input encoding
    std::string setInputEncoding( const std::string& );
    std::string getInputEncoding() const { return inputEncoding; };

    // set eos marker
    UnicodeString setEosMarker( const std::string& s = "<utt>") { UnicodeString t = eosmark; eosmark =  folia::UTF8ToUnicode(s); return t; };
    UnicodeString getEosMarker( ) const { return eosmark; }

    bool setSentencePerLineOutput( bool b=true ) { bool t = sentenceperlineoutput; sentenceperlineoutput = b; return t; };
    bool getSentencePerLineOutput() const { return sentenceperlineoutput; }

    bool setSentencePerLineInput( bool b=true ) { bool t = sentenceperlineinput; sentenceperlineinput = b; return t; };
    bool getSentencePerLineInput() const { return sentenceperlineinput; }

    bool getXMLOutput() const { return xmlout; }
    bool getXMLInput() const { return xmlin; }

    const std::string getTextClass( ) const { return inputclass; }
    const std::string setTextClass( const std::string& cls) {
      std::string res = inputclass;
      inputclass = cls;
      return res;
    }
    const std::string getInputClass( ) const { return inputclass; }
    const std::string setInputClass( const std::string& cls) {
      std::string res = inputclass;
      inputclass = cls;
      return res;
    }
    const std::string getOutputClass( ) const { return outputclass; }
    const std::string setOutputClass( const std::string& cls) {
      std::string res = outputclass;
      outputclass = cls;
      return res;
    }

    std::string getDocID() const { return docid; }
    std::string setDocID( const std::string& id ) {
      const std::string s = docid; docid = id; return s; }
    bool setXMLOutput( bool b ) {
      bool t = xmlout; xmlout = b; return t; }
    bool setXMLOutput( bool b, const std::string& id ) {
      setDocID( id ); return setXMLOutput(b); }
    bool setXMLInput( bool b ) { bool t = xmlin; xmlin = b; return t; }

    void outputTokens( std::ostream&, const std::vector<Token>& ,const bool continued=false) const; //continued should be set to true when outputTokens is invoked multiple times and it is not the first invokation
  private:
    void tokenizeWord( const UnicodeString&, bool);

    bool detectEos( size_t ) const;
    void detectSentenceBounds( const int offset = 0 );
    void detectQuotedSentenceBounds( const int offset = 0 );
    void detectQuoteBounds( const int );
    //Signal the tokeniser that a paragraph is detected
    void signalParagraph( bool b=true ) { paragraphsignal = b; };

    bool resolveQuote( int, const UnicodeString& );
    bool u_isquote( UChar32 ) const;
    std::string checkBOM( const std::string&, std::string& );
    bool readsettings( const std::string& );
    bool readrules( const std::string& );
    bool readfilters( const std::string& );
    bool readquotes( const std::string& );
    bool readeosmarkers( const std::string& );
    bool readabbreviations( const std::string&, UnicodeString& );

    void sortRules( std::vector<Rule *>&, std::vector<UnicodeString>& );
    void outputTokensDoc( folia::Document&, const std::vector<Token>& ) const;
    void outputTokensDoc_init( folia::Document& ) const;

    int outputTokensXML( folia::FoliaElement *, const std::vector<Token>& , int parCount=0 ) const;
    void tokenizeElement( folia::FoliaElement * );
    void tokenizeSentenceElement( folia::FoliaElement * );

    Quoting quotes;
    UnicodeFilter filter;
    UnicodeNormalizer normalizer;
    UnicodeString eosmarkers;
    std::string inputEncoding;

    UnicodeString eosmark;
    std::vector<Token> tokens;
    std::vector<Rule *> rules;
    TiCC::LogStream *theErrLog;

    //debug flag
    int tokDebug;

    //verbose tokenisation mode
    bool verbose;

    //detect sentence bounds?
    bool detectBounds;

    //detect quotes?
    bool detectQuotes;

    //filter special characters (default on)
    bool doFilter;

    //filter all punctuation characters (default off)
    bool doPunctFilter;

    //detect paragraphs?
    bool detectPar;

    //has a paragraph been signaled?
    bool paragraphsignal;

    //one sentence per line output
    bool sentenceperlineoutput;
    bool sentenceperlineinput;


    bool lowercase;
    bool uppercase;
    bool xmlout;
    bool xmlin;
    bool passthru;

    std::string settingsfilename;
    std::string docid; //document ID (UTF-8), necessary for XML output
    std::string inputclass; // class for folia text
    std::string outputclass; // class for folia text
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
