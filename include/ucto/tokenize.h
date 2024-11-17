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

#ifndef UCTO_TOKENIZE_H
#define UCTO_TOKENIZE_H

#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <stdexcept>
#include "libfolia/folia.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Unicode.h"
#include "ucto/setting.h"

class TextCat;

namespace Tokenizer {

  using namespace icu;

  const std::string Version();
  const std::string VersionName();

  enum TokenRole {
    NOROLE                      = 0,
    NOSPACE                     = 1,
    BEGINOFSENTENCE             = 2,
    ENDOFSENTENCE               = 4,
    NEWPARAGRAPH                = 8,
    BEGINQUOTE                  = 16,
    ENDQUOTE                    = 32,
    TEMPENDOFSENTENCE           = 64,
    LINEBREAK                   = 128
  };

  std::ostream& operator<<( std::ostream&, const TokenRole& );

  // setter
  inline TokenRole operator|( TokenRole T1, TokenRole T2 ){
    return (TokenRole)( (int)T1|(int)T2 );
  }

  inline TokenRole& operator|= ( TokenRole& T1, TokenRole T2 ){
    T1 = (T1 | T2);
    return T1;
  }

  // invert
  inline TokenRole operator~( TokenRole T1 ){
    return (TokenRole)~(int)T1;
  }

  // union
  inline TokenRole operator&( TokenRole T1, TokenRole T2 ){
    return (TokenRole)( (int)T1 & (int)T2 );
  }

  inline TokenRole& operator&=( TokenRole& T1, TokenRole T2 ){
    T1 = (T1 & T2);
    return T1;
  }

  class Token {
    friend std::ostream& operator<< (std::ostream&, const Token& );
  public:
    UnicodeString type;
    UnicodeString us;
    TokenRole role;
    Token( const UnicodeString&,
	   const UnicodeString&,
	   TokenRole role = NOROLE,
	   const std::string& = "" );
    Token( const UnicodeString&,
	   const UnicodeString&,
	   const std::string& = "" );
    std::string lang_code;                // ISO 639-3 language code
    std::string texttostring();
    std::string typetostring();
  };

  class TokenizerClass{
  protected:
    int linenum;
  public:
    TokenizerClass();
    ~TokenizerClass();
    bool init( const std::string&,
	       const std::string& ="" ); // init from a configfile
    bool init( const std::vector<std::string>&,
	       const std::string& ="" ); // init 1 or more languages
    bool initialize_textcat();
    bool reset( const std::string& = "default" );
    void setErrorLog( TiCC::LogStream *os );
    void setDebugLog( TiCC::LogStream *os );

    // Tokenize from input stream with text OR FoLiA to a FoLiA document
    folia::Document *tokenize_folia( const std::string& );
    // Tokenize from input stream with text to a FoLiA document (
    folia::Document *tokenize( std::istream& );

    // Tokenize from input stream with text OR FoLiA to a FoLiA document and
    //   save it
    void tokenize_folia( const std::string&, const std::string&  );

    // Tokenize from an input text stream to a token vector
    // (representing a sentence)
    // non greedy. Stops after the first full sentence is returned.
    // may be called multiple times until EOF
    std::vector<Token> tokenizeOneSentence( std::istream& );

    // tokenize from file to file
    void tokenize( const std::string&, const std::string& );

    //Tokenize from input stream to output stream
    void tokenize( std::istream&, std::ostream& );

    // Tokenize a line (a line is NOT just a sentence, but an arbitrary string
    //                  of characters, inclusive EOS markers, Newlines etc.)
    //
    // OR use popSentence() repeatedly to extract all sentences as vectors
    //    using getString() to extract the UTF8 value of that sentence
    // OR getSentences() to get ALL sentences as UTF8 strings in a vector
    void tokenizeLine( const UnicodeString&, const std::string& = "" );
    void tokenizeLine( const std::string&, const std::string& = "" );

    // extract 1 sentence from Token vector;
    std::vector<Token> popSentence();

    // convert the sentence in a token vector to a UnicodeString
    icu::UnicodeString getString( const std::vector<Token>& );
    // convert the sentence in a token vector to a string (UTF-8 encoded)
    std::string getUTF8String( const std::vector<Token>& );

    // extract all sentences as a vector of UnicodeStrings
    std::vector<icu::UnicodeString> getSentences();

    // extract all sentences as a vector of strings (UTF-8 encoded)
    std::vector<std::string> getUTF8Sentences();

    //Enable verbose mode
    bool setVerbose( bool b=true ) { bool t = verbose; verbose = b; return t; };
    bool getVerbose() const { return verbose; }

    //set debug value
    int setDebug( int d ) { int dd = tokDebug; tokDebug = d; return dd; };
    int getDebug() const { return tokDebug; }

    // set the commandline used
    void set_command( const std::string& c ){ _command =  c; };

    //set textcat debug value
    bool set_tc_debug( bool b );

    //Enable conversion of all output to lowercase
    bool setLowercase( bool b=true ) { bool t = lowercase; lowercase = b; if (b) uppercase = false; return t; };
    bool getLowercase() const { return lowercase; }

    //Enable passtru mode
    bool setPassThru( bool b=true ) { bool t = passthru; passthru = b; return t; };
    bool getPassThru() const { return passthru; }

    //Disable tag hints
    bool setNoTags( bool b=true ) { bool t = ignore_tag_hints;
      ignore_tag_hints = b;
      return t; };
    bool getNoTags() const { return ignore_tag_hints; }

    //Enable conversion of all output to uppercase
    bool setUppercase( bool b=true ) { bool t = uppercase; uppercase = b; if (b) lowercase = false; return t; };
    bool getUppercase() const { return uppercase; }

    //Enable sentence splitting only
    bool setSentenceSplit( bool b=true ) { bool t = splitOnly; splitOnly = b; return t; }
    bool getSentenceSplit() const { return splitOnly; }

    //Enable paragraph detection
    bool setParagraphDetection( bool b=true ) { bool t = detectPar; detectPar = b; return t; }
    bool getParagraphDetection() const { return detectPar; }

    //Enable quote detection
    bool setQuoteDetection( bool b=true ) { bool t = detectQuotes; detectQuotes = b; return t; }
    bool getQuoteDetection() const { return detectQuotes; }

    //Enable language detection returns false on failure!
    bool setLangDetection( bool=true );
    // check language detection return true when set AND availabe!
    bool getLangDetection() const { return doDetectLang; }

    //Enable filtering
    bool setFiltering( bool b=true ) {
      bool t = doFilter; doFilter = b; return t;
    }
    bool getFiltering() const { return doFilter; };

    //Enable word corrections (FoLiA only)
    bool setWordCorrection( bool b=true ) {
      bool t = doWordCorrection; doWordCorrection = b; return t;
    }
    bool getWordCorrection() const { return doWordCorrection; };

    //Enable punctuation filtering
    bool setPunctFilter( bool b=true ) {
      bool t = doPunctFilter; doPunctFilter = b; return t;
    }
    bool getPunctFilter() const { return doPunctFilter; };

    std::string setTextRedundancy( const std::string& );

    // set the desired separators.
    std::string setSeparators( const std::string& );

    // set normalization mode
    std::string setNormalization( const std::string& s ) {
      return normalizer.setMode( s );
    }
    std::string getNormalization() const { return normalizer.getMode(); };

    // set input encoding
    std::string setInputEncoding( const std::string& );
    const std::string& getInputEncoding() const { return inputEncoding; };

    void setLanguage( const std::string& l ){ default_language = l; };
    const std::string& getLanguage() const { return default_language; };

    // set eos marker: for Comptability. UttMarker variants are preferred
    UnicodeString setEosMarker( const std::string& s = "<utt>") { UnicodeString t = utt_mark; utt_mark = TiCC::UnicodeFromUTF8(s); return t; };
    const UnicodeString& getEosMarker( ) const { return utt_mark; }

    UnicodeString setUttMarker( const std::string& s = "<utt>") { UnicodeString t = utt_mark; utt_mark = TiCC::UnicodeFromUTF8(s); return t; };
    UnicodeString getUttMarker( ) const { return utt_mark; }

    bool setNormSet( const std::string& );

    bool setSentencePerLineOutput( bool b=true ) { bool t = sentenceperlineoutput; sentenceperlineoutput = b; return t; };
    bool getSentencePerLineOutput() const { return sentenceperlineoutput; }

    bool setSentencePerLineInput( bool b=true ) { bool t = sentenceperlineinput; sentenceperlineinput = b; return t; };
    bool getSentencePerLineInput() const { return sentenceperlineinput; }

    bool setXMLOutput( bool b ) {
      bool t = xmlout; xmlout = b; return t; }
    bool setXMLOutput( bool b, const std::string& id ) {
      setDocID( id ); return setXMLOutput(b); }
    bool getXMLOutput() const { return xmlout; }

    bool setXMLInput( bool b ) { bool t = xmlin; xmlin = b; return t; }
    bool getXMLInput() const { return xmlin; }
    bool setUndLang( bool b ){ bool r = und_language; und_language = b; return r; };
    bool getUndLang(){ return und_language; };

    const std::string& getInputClass( ) const { return inputclass; }
    const std::string setInputClass( const std::string& cls) {
      std::string res = inputclass;
      inputclass = cls;
      return res;
    }
    const std::string& getOutputClass( ) const { return outputclass; }
    const std::string setOutputClass( const std::string& cls) {
      std::string res = outputclass;
      outputclass = cls;
      return res;
    }
    bool setCopyClass( bool b ){
      bool ret = copyclass;
      copyclass = b;
      return ret;
    }
    const std::string& getDocID() const { return docid; }
    std::string setDocID( const std::string& );

    bool get_setting_info( const std::string&,
			   std::string&,
			   std::string& ) const;
    std::string get_data_version() const;

    folia::processor *init_provenance( folia::Document *,
				       folia::processor * =0 ) const;
    folia::processor *add_provenance_passthru( folia::Document *,
					       folia::processor * =0 ) const;
    folia::processor *add_provenance_undetermined( folia::Document *,
						   folia::processor * =0 ) const;
    folia::processor *add_provenance_data( folia::Document *,
					   folia::processor * =0 ) const;
    folia::processor *add_provenance_setting( folia::Document *,
					      folia::processor * =0 ) const;
    folia::processor *add_provenance_structure( folia::Document *,
						folia::processor * =0 ) const;
    folia::processor *add_provenance_structure( folia::Document *,
						const folia::AnnotationType,
						folia::processor * =0 ) const;
    bool ucto_re_run() const { return already_tokenized; };
    std::vector<Token> correct_elements( folia::FoliaElement *,
					 const std::vector<folia::FoliaElement*>& );

  private:

    TokenizerClass( const TokenizerClass& ) = delete; // inhibit copies
    TokenizerClass& operator=( const TokenizerClass& ) = delete; // inhibit copies

    void passthruLine( const UnicodeString&, bool& );
    void passthruLine( const std::string&, bool& );
    std::vector<icu::UnicodeString> sentence_split( const icu::UnicodeString& );
    std::string detect( const icu::UnicodeString& );
    folia::Document *start_document( const std::string& ) const;
    folia::FoliaElement *append_to_folia( folia::FoliaElement *root,
					  const std::vector<Token>& tv,
					  int& p_count ) const;

    std::vector<folia::Word*> append_to_sentence( folia::Sentence *,
						  const std::vector<Token>& ) const;
    void correct_element( folia::FoliaElement *,
			  const std::vector<Token>&,
			  const std::string& ) const;

    void handle_one_sentence( folia::Sentence *, int& );
    void handle_one_paragraph( folia::Paragraph *, int& );
    void handle_one_text_parent( folia::FoliaElement *, int& );

    //Processes tokens and initialises the sentence buffer. Returns the amount of sentences found
    int countSentences(bool forceentirebuffer = false);
    //count the number of sentences (only after detectSentenceBounds) (does some extra validation as well)
    int flushSentences( int, const std::string& = "default" );
    //Flush n sentences from buffer (does some extra validation as well)

    icu::UnicodeString outputTokens( const std::vector<Token>&,
				     const bool=false ) const;
    void add_rule( const UnicodeString&,
		   const std::vector<UnicodeString>& );
    void tokenizeWord( const UnicodeString&,
		       bool,
		       const std::string&,
		       const UnicodeString& ="" );
    int internal_tokenize_line( const UnicodeString&,
				const std::string& );

    void tokenize_one_line( const UnicodeString&,
			    bool&,
			    const std::string& = "" );

    bool detectEos( size_t, const UnicodeString&, const Quoting& ) const;
    void detectSentenceBounds( const int offset,
			       const std::string& = "default" );
    void detectQuotedSentenceBounds( const int offset,
				     const std::string& = "default" );
    void detectQuoteBounds( const int,
			    Quoting& );

    bool resolveQuote( int, const UnicodeString&, Quoting& );
    bool u_isquote( UChar32,
		    const Quoting& ) const;
    std::string checkBOM( std::istream& );
    void outputTokensDoc_init( folia::Document& ) const;

    void appendText( folia::FoliaElement * ) const;

    TiCC::UnicodeNormalizer normalizer;
    std::string inputEncoding;

    const UnicodeString& detect_type( UChar32 );
    bool is_separator( UChar32 );
    std::set<UChar32> separators;
    bool space_separated;

    UnicodeString utt_mark;
    std::vector<Token> tokens;
    std::set<UnicodeString> norm_set;
    TiCC::LogStream *theErrLog;
    TiCC::LogStream *theDbgLog;

    bool und_language;
    std::string default_language;
    std::string document_language; // in case of an input FoLiA document
    std::map<std::string,Setting*> settings;
    std::string _command; // original commandline
    //debug flag
    int tokDebug;

    //verbose tokenisation mode
    bool verbose;

    //detect quotes?
    bool detectQuotes;

    //filter special characters (default on)
    bool doFilter;

    //filter all punctuation characters (default off)
    bool doPunctFilter;

    //allow correction of FoLiA Word elements
    bool doWordCorrection;

    // only sentence spliiting?
    bool splitOnly;

    //detect paragraphs?
    bool detectPar;

    //has a paragraph been signaled?
    bool paragraphsignal;
    bool paragraphsignal_next;

    //has do we attempt to assign languages?
    bool doDetectLang;

    //has do we percolate text up from <w> to <s> and <p> nodes? (FoLiA)
    // values should be: 'full', 'minimal' or 'none'
    std::string text_redundancy;

    //one sentence per line output
    bool sentenceperlineoutput;
    bool sentenceperlineinput;

    bool copyclass;
    bool lowercase;
    bool uppercase;
    bool xmlout;
    bool xmlin;
    bool passthru;
    bool ignore_tag_hints;
    mutable folia::processor *ucto_processor;
    mutable bool already_tokenized; // set when ucto is called again on tokenized FoLiA
    std::string docid; //document ID (UTF-8), necessary for XML output
    std::string inputclass; // class for folia text
    std::string outputclass; // class for folia text
    std::string data_version; // the version of uctodata
    TextCat *text_cat;
    folia::TextPolicy text_policy;
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

  // extract the language assigned to this vector, if any...
  // will return "" if indetermined.
  std::string get_language( const std::vector<Token>& );
  // set the language on a FoliaElement
  void set_language( folia::FoliaElement*, const std::string& );
}
#endif
