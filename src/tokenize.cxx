/*
  Copyright (c) 2006 - 2019
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

#include "ucto/tokenize.h"

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "config.h"
#include "unicode/schriter.h"
#include "unicode/ucnv.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/Unicode.h"
#include "ucto/my_textcat.h"

#define DO_READLINE
#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else
#    undef DO_READLINE
#  endif /* !defined(HAVE_READLINE_H) */
#else
#  undef DO_READLINE
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
#endif /* HAVE_READLINE_HISTORY */

using namespace std;

#define LOG *TiCC::Log(theErrLog)

namespace Tokenizer {

  using namespace icu;
  using TiCC::operator<<;

  const string ISO_SET = "http://raw.github.com/proycon/folia/master/setdefinitions/iso639_3.foliaset";

  const std::string Version() { return VERSION; }
  const std::string VersionName() { return PACKAGE_STRING; }

  class uRangeError: public std::out_of_range {
  public:
    explicit uRangeError( const string& s ): out_of_range( "ucto: out of range:" + s ){};
  };

  class uLogicError: public std::logic_error {
  public:
    explicit uLogicError( const string& s ): logic_error( "ucto: logic error:" + s ){};
  };

  class uCodingError: public std::runtime_error {
  public:
    explicit uCodingError( const string& s ): runtime_error( "ucto: coding problem:" + s ){};
  };


  UnicodeString convert( const string& line,
			 const string& inputEncoding ){
    UnicodeString result;
    if ( !line.empty() ){
      try {
	result = UnicodeString( line.c_str(),
				line.length(),
				inputEncoding.c_str() );
      }
      catch ( exception &e) {
	throw uCodingError( "Unexpected character found in input. " +
			    string(e.what()) + "Make sure input is valid: " +
			    inputEncoding );
      }
      if ( result.isBogus() ){
	throw uCodingError( "string decoding failed: (invalid inputEncoding '"
			    + inputEncoding + "' ?)" );
      }
    }
    return result;
  }

  const UnicodeString type_space = "SPACE";
  const UnicodeString type_currency = "CURRENCY";
  const UnicodeString type_emoticon = "EMOTICON";
  const UnicodeString type_picto = "PICTOGRAM";
  const UnicodeString type_word = "WORD";
  const UnicodeString type_symbol = "SYMBOL";
  const UnicodeString type_punctuation = "PUNCTUATION";
  const UnicodeString type_number = "NUMBER";
  const UnicodeString type_unknown = "UNKNOWN";

  Token::Token( const UnicodeString& _type,
		const UnicodeString& _s,
		TokenRole _role, const string& _lang_code ):
    type(_type), us(_s), role(_role), lang_code(_lang_code) {
    //    cerr << "Created " << *this << endl;
  }


  std::string Token::texttostring() { return TiCC::UnicodeToUTF8(us); }
  std::string Token::typetostring() { return TiCC::UnicodeToUTF8(type); }

  ostream& operator<< (std::ostream& os, const Token& t ){
    os << t.type << " : " << t.role  << ":" << t.us << " (" << t.lang_code << ")";
    return os;
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

  TokenizerClass::TokenizerClass():
    linenum(0),
    inputEncoding( "UTF-8" ),
    eosmark("<utt>"),
    tokDebug(0),
    verbose(false),
    detectQuotes(false),
    doFilter(true),
    doPunctFilter(false),
    splitOnly( false ),
    detectPar(true),
    paragraphsignal(true),
    doDetectLang(false),
    text_redundancy("minimal"),
    sentenceperlineoutput(false),
    sentenceperlineinput(false),
    lowercase(false),
    uppercase(false),
    xmlout(false),
    xmlin(false),
    passthru(false),
    inputclass("current"),
    outputclass("current"),
    tc( 0 )
  {
    theErrLog = new TiCC::LogStream(cerr, "ucto" );
    theErrLog->setstamp( StampMessage );
#ifdef HAVE_TEXTCAT
    string textcat_cfg = string(SYSCONF_PATH) + "/ucto/textcat.cfg";
    tc = new TextCat( textcat_cfg, theErrLog );
    //    tc->set_debug( true );
    LOG << "configured TEXTCAT( " << textcat_cfg << " )" << endl;
    // ifstream is( textcat_cfg );
    // string line;
    // while ( getline( is, line ) ){
    //   LOG << line << endl;
    //   vector<string> v = TiCC::split( line );
    //   if ( v.size()==2 && v[1] == "nld" ){
    // 	LOG << "voor nederlands: " << endl;
    //     ifstream is2( v[0] );
    // 	string line2;
    // 	while ( getline( is2, line2 ) ){
    // 	  LOG << line2 << endl;
    // 	  break;
    // 	}
    // 	LOG << "   done with nederlands" << endl;
    //   }
    // }
#else
    LOG << "NO TEXTCAT SUPPORT!" << endl;
#endif
  }

  TokenizerClass::~TokenizerClass(){
    Setting *d = 0;
    for ( const auto& s : settings ){
      if ( s.first == "default" ){
	// the 'default' may also return as a real 'language'
	// avoid deleting it twice
	d = s.second;
	delete d;
      }
      if ( s.second != d ){
	delete s.second;
      }

    }
    delete theErrLog;
    delete tc;
  }

  bool TokenizerClass::reset( const string& lang ){
    tokens.clear();
    settings[lang]->quotes.clearStack();
    return true;
  }

  bool TokenizerClass::setNormSet( const std::string& values ){
    vector<string> parts = TiCC::split_at( values, "," );
    for ( const auto& val : parts ){
      norm_set.insert( TiCC::UnicodeFromUTF8( val ) );
    }
    return true;
  }

  void TokenizerClass::setErrorLog( TiCC::LogStream *os ) {
    if ( theErrLog != os ){
      tc->set_debug_stream( os );
      delete theErrLog;
    }
    theErrLog = os;
  }

  string TokenizerClass::setInputEncoding( const std::string& enc ){
    string old = inputEncoding;
    inputEncoding = enc;
    return old;
  }

  string TokenizerClass::setTextRedundancy( const std::string& tr ){
    if ( tr == "none" || tr == "minimal" || tr == "full" ){
      string s = text_redundancy;
      text_redundancy = tr;
      return s;
    }
    else {
      throw runtime_error( "illegal value '" + tr + "' for textredundancy. "
			   "expected 'full', 'minimal' or 'none'." );
    }
  }

  bool TokenizerClass::set_tc_debug( bool b ){
    if ( !tc ){
      throw logic_error( "attempt to set debug on uninitialized TextClass object" );
    }
    else {
      return tc->set_debug( b );
    }
  }

  string fixup_UTF16( string& input_line, const string& encoding ){
    string line = input_line;
    // some hackery to handle exotic input. UTF-16 but also CR at end.
    string::size_type pos = line.rfind( '\r' );
    if ( pos != string::npos ){
      line.erase( pos );
    }
    if ( line.size() > 0 && line[0] == 0 ){
      // when processing UTF16LE, '0' bytes show up at pos 0
      // we discard them, not for UTF16BE!
      // this works on Linux with GCC (atm)
      if ( encoding != "UTF16BE" ){
	line.erase(0,1);
      }
    }
    if ( line.size() > 0 && encoding == "UTF16BE" &&
	 line.back() == 0 ){
      // when processing UTF16BE, '0' bytes show up at the end
      // we discard them.
      // this works on Linux with GCC (atm)
      line.erase(line.size()-1);
    }
    return line;
  }


  folia::Document *TokenizerClass::start_document( const string& id ) const {
    folia::Document *doc = new folia::Document( "xml:id='" + id + "'" );
    if ( default_language != "none" ){
      doc->set_metadata( "language", default_language );
    }
    doc->addStyle( "text/xsl", "folia.xsl" );
    if ( tokDebug > 3 ){
      LOG << "start document!!!" << endl;
    }
    if ( passthru ){
      doc->declare( folia::AnnotationType::TOKEN, "passthru", "annotator='ucto', annotatortype='auto', datetime='now()'" );
    }
    else {
      for ( const auto& s : settings ){
	if ( tokDebug > 3 ){
	  LOG << "language: " << s.first << endl;
	}
	doc->declare( folia::AnnotationType::TOKEN,
		      s.second->set_file,
		      "annotator='ucto', annotatortype='auto', datetime='now()'");
	if ( tokDebug > 3 ){
	  LOG << "added token-annotation for: '"
	      << s.second->set_file << "'" << endl;
	}
	doc->declare( folia::AnnotationType::LANG,
		      ISO_SET, "annotator='ucto'" );
      }
    }
    folia::KWargs args;
    args["xml:id"] = doc->id() + ".text";
    folia::Text *text = new folia::Text( args );
    doc->setRoot( text );
    return doc;
  }

  void TokenizerClass::tokenize_one_line( const UnicodeString& input_line,
					  bool& bos ){
    if ( passthru ){
      passthruLine( input_line, bos );
    }
    else {
      string language = "default";
      if ( tc ){
	UnicodeString temp = input_line;
	temp.findAndReplace( eosmark, "" );
	temp.toLower();
	if ( tokDebug > 3 ){
	  LOG << "use textCat to guess language from: "
	      << temp << endl;
	}
	language = tc->get_language( TiCC::UnicodeToUTF8(temp) );
	if ( settings.find( language ) != settings.end() ){
	  if ( tokDebug > 3 ){
	    LOG << "found a supported language: " << language << endl;
	  }
	}
	else {
	  if ( tokDebug > 3 ){
	    LOG << "found an unsupported language: " << language << endl;
	  }
	  language = "default";
	}
      }
      tokenizeLine( input_line, language );
    }
  }

  vector<Token> TokenizerClass::tokenizeOneSentence( istream& IN ){
    int numS = countSentences(); //count full sentences in token buffer
    if ( numS > 0 ) { // still some sentences in the buffer
      if  (tokDebug > 0) {
	LOG << "[tokenizeOneSentence] " << numS
	    << " sentence(s) in buffer, processing..." << endl;
      }
      return popSentence( );
    }
    bool done = false;
    bool bos = true;
    string line;
    do {
      done = !getline( IN, line );
      UnicodeString input_line;
      if ( !done ){
	++linenum;
	if (tokDebug > 0) {
	  LOG << "[tokenize] Read input line " << linenum
	      << "-: '" << TiCC::format_nonascii( line ) << "'" << endl;
	}
	string tmp_line = fixup_UTF16( line, inputEncoding );
	if ( tokDebug > 0
	     && tmp_line != line ){
	  LOG << "After fixup, input_line= '"
	      << TiCC::format_nonascii( tmp_line ) << "'" << endl;
	}
	input_line = convert( tmp_line, inputEncoding );
	if ( sentenceperlineinput ){
	  input_line += " " + eosmark;
	}
      }
      if ( done || input_line.isEmpty() ){
	//Signal the tokeniser that a paragraph is detected
	paragraphsignal = true;
	numS = countSentences(true); //count full sentences in token buffer,
	// setting explicit END_OF_SENTENCE
      }
      else {
	tokenize_one_line( input_line, bos );
	numS = countSentences(); //count full sentences in token buffer
      }
      if ( numS > 0 ) {
	// 1 or more sentences in the buffer.
	// extract the first 1
	if  (tokDebug > 0) {
	  LOG << "[tokenizeOneSentence] " << numS << " sentence(s) in buffer, processing first one..." << endl;
	}
	return popSentence();
      }
      else {
	if  (tokDebug > 0) {
	  LOG << "[tokenizeOneSentence] No sentence yet, reading on..." << endl;
	}
      }
    } while (!done);
    vector<Token> result;
    return result;
  }

  folia::Document *TokenizerClass::tokenize( istream& IN ) {
    inputEncoding = checkBOM( IN );
    folia::Document *doc = start_document( docid );
    folia::FoliaElement *root = doc->doc()->index(0);
    int parCount = 0;
    vector<Token> buffer;
    do {
      if ( tokDebug > 0 ){
	LOG << "[tokenize] looping on stream" << endl;
      }
      vector<Token> v = tokenizeOneSentence( IN );
      if ( !v.empty() ){
	if ( tokDebug > 1 ){
	  LOG << "[tokenize] sentence=" << v << endl;
	}
	root = append_to_folia( root, v, parCount );
      }
    }
    while ( IN );
    if ( tokDebug > 0 ){
      LOG << "[tokenize] end of stream reached" << endl;
    }
    if (!buffer.empty()){
      append_to_folia( root, buffer, parCount);
    }
    return doc;
  }

  void TokenizerClass::tokenize( const string& ifile, const string& ofile ){
    ostream *OUT = NULL;
    if ( ofile.empty() )
      OUT = &cout;
    else {
      OUT = new ofstream( ofile );
    }

    istream *IN = NULL;
    if ( xmlin ){
      folia::Document *doc = tokenize_folia( ifile );
      *OUT << *doc << endl;
      delete doc;
    }
    else {
      if ( ifile.empty() )
	IN = &cin;
      else {
	IN = new ifstream( ifile );
	if ( !IN || !IN->good() ){
	  cerr << "ucto: problems opening inputfile " << ifile << endl;
	  cerr << "ucto: Courageously refusing to start..."  << endl;
	  throw runtime_error( "unable to find or read file: '" + ifile + "'" );
	}
      }
      this->tokenize( *IN, *OUT );
    }
    if ( IN != &cin ) delete IN;
    if ( OUT != &cout ) delete OUT;
  }

  void TokenizerClass::tokenize( istream& IN, ostream& OUT) {
    if (xmlout) {
      folia::Document *doc = tokenize( IN );
      OUT << doc << endl;
      delete doc;
    }
#ifdef DO_READLINE
    else if ( &IN == &cin && isatty(0) ){
      // interactive use on a terminal (quite a hack..)
      const char *prompt = "ucto> ";
      string line;
      int i = 0;
      while ( true ){
	string data;
	char *input = readline( prompt );
	if ( !input ){
	  break;
	}
	line = input;
	sentenceperlineinput = true;
	if ( line.empty() ){
	  free( input );
	  continue;
	}
	else {
	  add_history( input );
	  free( input );
	  data += line + " ";
	}
	if ( !data.empty() ){
	  vector<Token> v = tokenizeOneSentence( IN );
	  while( !v.empty() ){
	    outputTokens( OUT, v , (i>0) );
	    ++i;
	    v = tokenizeOneSentence( IN );
	  }
	  OUT << endl;
	}
      }
    }
#endif
    else {
      int i = 0;
      inputEncoding = checkBOM( IN );
      do {
	if ( tokDebug > 0 ){
	  LOG << "[tokenize] looping on stream" << endl;
	}
	vector<Token> v = tokenizeOneSentence( IN );
	while( !v.empty() ){
	  outputTokens( OUT, v , (i>0) );
	  ++i;
	  v = tokenizeOneSentence( IN );
	}
      } while ( IN );
      if ( tokDebug > 0 ){
	LOG << "[tokenize] end_of_stream" << endl;
      }
      OUT << endl;
    }
  }

  void appendText( folia::FoliaElement *root,
		   const string& outputclass  ){
    // set the textcontent of root to that of it's children
    if ( root->hastext( outputclass ) ){
      // there is already text, bail out.
      return;
    }
    if ( root->isSubClass( folia::Linebreak_t ) ){
      // exception
      return;
    }
    UnicodeString utxt = root->text( outputclass, false, false );
    // so get Untokenized text from the children, and set it
    root->settext( TiCC::UnicodeToUTF8(utxt), outputclass );
  }

  void removeText( folia::FoliaElement *root,
		   const string& outputclass  ){
    // remove the textcontent in outputclass of root
    root->cleartextcontent( outputclass );
  }

  void set_language( folia::FoliaElement* e, const string& lan ){
    // set or reset the language: append a LangAnnotation child of class 'lan'
    folia::KWargs args;
    args["class"] = lan;
    args["set"] = ISO_SET;
    folia::LangAnnotation *node = new folia::LangAnnotation( e->doc() );
    node->setAttributes( args );
    e->replace( node );
  }

  string get_parent_id( folia::FoliaElement *el ){
    if ( !el ){
      return "";
    }
    else if ( !el->id().empty() ){
      return el->id();
    }
    else {
      return get_parent_id( el->parent() );
    }
  }

  folia::FoliaElement *TokenizerClass::append_to_folia( folia::FoliaElement *root,
							const vector<Token>& tv,
							int& p_count ) const {
    if ( !root || !root->doc() ){
      throw logic_error( "missing root" );
    }
    if  ( tokDebug > 5 ){
      LOG << "append_to_folia, root = " << root << endl;
      LOG << "tokens=\n" << tv << endl;
    }
    folia::KWargs args;
    if ( (tv[0].role & NEWPARAGRAPH) ) {
      if  ( tokDebug > 5 ){
	LOG << "append_to_folia, NEW paragraph " << endl;
      }
      args["xml:id"] = root->doc()->id() + ".p." + TiCC::toString(++p_count);
      folia::Paragraph *p = new folia::Paragraph( args, root->doc() );
      if ( root->element_id() == folia::Text_t ){
	if  ( tokDebug > 5 ){
	  LOG << "append_to_folia, add paragraph to Text" << endl;
	}
	root->append( p );
      }
      else {
	// root is a paragraph, which is done now.
	if ( text_redundancy == "full" ){
	  root->settext( root->str(outputclass), outputclass);
	}
	if  ( tokDebug > 5 ){
	  LOG << "append_to_folia, add paragraph to parent of " << root << endl;
	}
	root = root->parent();
	root->append( p );
      }
      root = p;
    }
    args.clear();
    args["generate_id"] = root->id();
    folia::Sentence *s = new folia::Sentence( args, root->doc() );
    root->append( s );
    if  ( tokDebug > 5 ){
      LOG << "append_to_folia, created Sentence" << s << endl;
    }
    string tok_set;
    string lang = get_language( tv );
    if ( lang != "default" ){
      tok_set = "tokconfig-" + lang;
      set_language( s, lang );
    }
    else if ( default_language != "none" ){
      tok_set = "tokconfig-" + default_language;
    }
    vector<folia::Word*> wv = add_words( s, tok_set, tv );
    return root;
  }

  vector<folia::Word*> TokenizerClass::add_words( folia::Sentence* s,
						  const string& tok_set,
						  const vector<Token>& toks ) const{
    vector<folia::Word*> wv;
    folia::FoliaElement *root = s;
    folia::Document *doc = s->doc();
    if ( tokDebug > 5 ){
      LOG << "add_words\n" << toks << endl;
    }
    for ( size_t i=0; i < toks.size(); ++i ){
      const auto& tok = toks[i];
      if ( tokDebug > 5 ){
	LOG << "add_result\n" << tok << endl;
      }
      if ( tok.role & BEGINQUOTE ){
	if  (tokDebug > 5 ) {
	  LOG << "[add_words] Creating quote element" << endl;
	}
	folia::KWargs args;
	string id = get_parent_id(root);
	if ( !id.empty() ){
	  args["generate_id"] = id;
	}
	folia::FoliaElement *q = new folia::Quote( args, doc );
	root->append( q );
	// might need a new Sentence
	if ( i+1 < toks.size()
	     && toks[i+1].role & BEGINOFSENTENCE ){
	  folia::Sentence *ns = new folia::Sentence( args, doc );
	  q->append( ns );
	  root = ns;
	}
	else {
	  root = q;
	}
      }
      else if ( (tok.role & BEGINOFSENTENCE)
		&& root != s
		&& root->element_id() == folia::Sentence_t ){
	// Ok, another Sentence in a quote
	if ( i > 0 && !(toks[i-1].role & BEGINQUOTE) ){
	// close the current one, and start a new one.
	  // except when it is implicit created by a QUOTE
	  if ( tokDebug > 5 ){
	    LOG << "[add_words] next embedded sentence" << endl;
	  }
	  // honour text_redundancy on the Sentence
	  if ( text_redundancy == "full" ){
	    appendText( root, outputclass );
	  }
	  else if ( text_redundancy == "none" ){
	    removeText( root, outputclass );
	  }
	  root = root->parent();
	  folia::KWargs args;
	  string id = root->id();
	  if ( !id.empty() ){
	    args["generate_id"] = id;
	  }
	  folia::Sentence *ns = new folia::Sentence( args, doc );
	  root->append( ns );
	  root = ns;
	}
      }
      folia::KWargs args;
      string ids = get_parent_id( root );
      if ( !ids.empty() ){
	args["generate_id"] = ids;
      }
      args["class"] = TiCC::UnicodeToUTF8(tok.type);
      if ( tok.role & NOSPACE ){
	args["space"] = "no";
      }
      if ( outputclass != "current" ){
	args["textclass"] = outputclass;
      }
      if ( !tok_set.empty() ){
	args["set"] = tok_set;
      }
      folia::Word *w;
#pragma omp critical (foliaupdate)
      {
	UnicodeString ws = tok.us;
	if (lowercase) {
	  ws = ws.toLower();
	}
	else if (uppercase) {
	  ws = ws.toUpper();
	}
	if ( tokDebug > 5 ){
	  LOG << "create Word(" << args << ") = " << ws << endl;
	}
	try {
	  w = new folia::Word( args, doc );
	}
	catch ( const exception& e ){
	  cerr << "WHAT=" << e.what() << endl;
	  exit(EXIT_FAILURE);
	}
	w->setutext( ws, outputclass );
	if ( tokDebug > 5 ){
	  LOG << "add_result, create a word, done:" << w << endl;
	}
	root->append( w );
      }
      wv.push_back( w );
      if ( tok.role & ENDQUOTE ){
	if ( i > 0
	     && toks[i-1].role & ENDOFSENTENCE ){
	  // end of quote implies with embedded Sentence
	  if ( tokDebug > 5 ){
	    LOG << "[add_words] End of quote" << endl;
	  }
	  // honour text_redundancy on the Sentence
	  if ( text_redundancy == "full" ){
	    appendText( root->parent(), outputclass );
	  }
	  else if ( text_redundancy == "none" ){
	    removeText( root->parent(), outputclass );
	  }
	  root = root->parent()->parent(); // so close Sentence too
	}
	else {
	  root = root->parent();
	}
      }
    }
    if ( text_redundancy == "full" ){
      appendText( s, outputclass );
    }
    else if ( text_redundancy == "none" ){
      removeText( s, outputclass );
    }
    return wv;
  }

  void TokenizerClass::append_to_sentence( folia::Sentence *sent,
					   const vector<Token>& toks ) const {
    string s_la;
    if ( sent->hasannotation<folia::LangAnnotation>() ){
      s_la = sent->annotation<folia::LangAnnotation>()->cls();
    }
    string tc_lc = toks[0].lang_code;
    if ( tokDebug >= 0 ){
      LOG << "append_to_sentence()" << endl;
      LOG << "language code= " << tc_lc << endl;
      LOG << "default language = " << default_language << endl;
      LOG << "sentence language = " << s_la << endl;
    }
    if ( ( !s_la.empty() && s_la != default_language
	   && default_language != "none"
	   && settings.find(s_la) == settings.end() )
	 || ( tc_lc != default_language
	      && default_language != "none"
	      && settings.find(tc_lc) == settings.end() ) ){
      // skip
      // if ( tc_lc != default_language ){
      // 	set_language( sent, tc_lc );
      // }
      // cerr << "sla" << (settings.find(s_la) == settings.end() )<< endl;
      // cerr << "tc_lc" << (settings.find(tc_lc) == settings.end()) << endl;
      if ( tokDebug > 0 ){
	LOG << "append_to_sentence() SKIP a sentence: " << s_la << endl;
      }
    }
    else {
      // add tokenization, when applicable
      string tok_set;
      if ( toks[0].lang_code != "default" ){
	tok_set = "tokconfig-" + tc_lc;
      }
      else if (default_language != "none" ) {
	tok_set = "tokconfig-" + default_language;
      }
      if ( !sent->doc()->isDeclared( folia::AnnotationType::LANG ) ){
	sent->doc()->declare( folia::AnnotationType::LANG,
			    ISO_SET, "annotator='ucto'" );
      }
      if ( !tok_set.empty() ){
	sent->doc()->declare( folia::AnnotationType::TOKEN,
			      tok_set,
			      "annotator='ucto', annotatortype='auto', datetime='now()'");
      }
      vector<folia::Word*> wv = add_words( sent, tok_set, toks );
    }
  }

  void TokenizerClass::handle_one_sentence( folia::Sentence *s,
					    int& sentence_done ){
    // check feasability
    if ( inputclass != outputclass && outputclass == "current" ){
      if ( s->hastext( outputclass ) ){
	throw uLogicError( "cannot set text with class='current' on node "
			   + s->id() +
			   " because it already has text in that class." );
      }
    }
    vector<folia::Word*> wv;
    wv = s->words( inputclass );
    if ( wv.empty() ){
      wv = s->words();
    }
    if ( !wv.empty() ){
      // there are already words.
    }
    else {
      string text = s->str(inputclass);
      if ( tokDebug > 0 ){
	LOG << "handle_one_sentence() from string: '" << text << "'" << endl;
      }
      tokenizeLine( text );
      vector<Token> sent = popSentence();
      while ( sent.size() > 0 ){
	append_to_sentence( s, sent );
	++sentence_done;
	sent = popSentence();
      }
    }
    if ( text_redundancy == "full" ){
      appendText( s, outputclass );
    }
    else if ( text_redundancy == "none" ){
      removeText( s, outputclass );
    }
  }

  void TokenizerClass::handle_one_paragraph( folia::Paragraph *p,
					     int& sentence_done ){
    // a Paragraph may contain both Word and Sentence nodes
    // if so, the Sentences should be handled separately
    vector<folia::Word*> wv = p->select<folia::Word>(false);
    vector<folia::Sentence*> sv = p->select<folia::Sentence>(false);
    if ( tokDebug > 1 ){
      LOG << "found some Words " << wv << endl;
      LOG << "found some Sentences " << sv << endl;
    }
    if ( sv.empty() ){
      // No Sentence, so only words OR just text
      string text = p->str(inputclass);
      if ( tokDebug > 0 ){
	LOG << "handle_one_paragraph:" << text << endl;
      }
      tokenizeLine( text );
      vector<Token> toks = popSentence();
      while ( !toks.empty() ){
	folia::KWargs args;
	string p_id = p->id();
	if ( !p_id.empty() ){
	  args["generate_id"] = p_id;
	}
	folia::Sentence *s = new folia::Sentence( args, p->doc() );
	p->append( s );
	append_to_sentence( s, toks );
	++sentence_done;
	toks = popSentence();
      }
    }
    else {
      // For now wu just IGNORE the loose words (backward compatability)
      for ( const auto& s : sv ){
	handle_one_sentence( s, sentence_done );
      }
    }
  }

  void TokenizerClass::handle_one_text_parent( folia::FoliaElement *e,
					       int& sentence_done ){
    ///
    /// input is a FoLiA element @e containing text, direct or deeper
    /// this can be a Word, Sentence, Paragraph or some other element
    /// In the latter case, we construct a Sentene from the text, and
    /// a Paragraph if more then one Sentence is found
    ///
    if ( inputclass != outputclass && outputclass == "current" ){
      if ( e->hastext( outputclass ) ){
	throw uLogicError( "cannot set text with class='current' on node "
			   + e->id() +
			   " because it already has text in that class." );
      }
    }
    if ( e->xmltag() == "w" ){
      // SKIP! already tokenized into words!
    }
    else if ( e->xmltag() == "s" ){
      // OK a text in a sentence
      if ( tokDebug > 2 ){
	LOG << "found text in a sentence " << e << endl;
      }
      handle_one_sentence( dynamic_cast<folia::Sentence*>(e),
			   ++sentence_done );
    }
    else if ( e->xmltag() == "p" ){
      // OK a longer text in some paragraph, maybe more sentences
      if ( tokDebug > 2 ){
	LOG << "found text in a paragraph " << e << endl;
      }
      handle_one_paragraph( dynamic_cast<folia::Paragraph*>(e),
			    sentence_done );
    }
    else {
      // Some text outside word, paragraphs or sentences (yet)
      // mabe <div> or <note> or such
      // there may be Paragraph, Word and Sentence nodes
      // if so, Paragraphs and Sentences should be handled separately
      vector<folia::Word*> wv = e->select<folia::Word>(false);
      vector<folia::Sentence*> sv = e->select<folia::Sentence>(false);
      vector<folia::Paragraph*> pv = e->select<folia::Paragraph>(false);
      if ( tokDebug > 1 ){
	LOG << "found some Words " << wv << endl;
	LOG << "found some Sentences " << sv << endl;
	LOG << "found some Paragraphs " << pv << endl;
      }
      if ( pv.empty() && sv.empty() ){
	// just words or text
	string text = e->str(inputclass);
	if ( tokDebug > 1 ){
	  LOG << "frog-" << e->xmltag() << ":" << text << endl;
	}
	tokenizeLine( text );
	vector<vector<Token>> sents;
	vector<Token> toks = popSentence();
	while ( toks.size() > 0 ){
	  sents.push_back( toks );
	  toks = popSentence();
	}
	if ( sents.size() > 1 ){
	  // multiple sentences. We need an extra Paragraph.
	  // But first check if this is allowed!
	  folia::FoliaElement *rt;
	  if ( e->acceptable(folia::Paragraph_t) ){
	    folia::KWargs args;
	    string e_id = e->id();
	    if ( !e_id.empty() ){
	      args["generate_id"] = e_id;
	    }
	    folia::Paragraph *p = new folia::Paragraph( args, e->doc() );
	    e->append( p );
	    rt = p;
	  }
	  else {
	    rt = e;
	  }
	  for ( const auto& sent : sents ){
	    folia::KWargs args;
	    string p_id = rt->id();
	    if ( !p_id.empty() ){
	      args["generate_id"] = p_id;
	    }
	    folia::Sentence *s = new folia::Sentence( args, e->doc() );
	    append_to_sentence( s, sent );
	    ++sentence_done;
	    if  (tokDebug > 0){
	      LOG << "created a new sentence: " << s << endl;
	    }
	    rt->append( s );
	  }
	}
	else {
	  // 1 sentence, connect directly.
	  folia::KWargs args;
	  string e_id = e->id();
	  if ( e_id.empty() ){
	    e_id = e->generateId( e->xmltag() );
	    args["xml:id"] = e_id + ".s.1";
	  }
	  else {
	    args["generate_id"] = e_id;
	  }
	  folia::Sentence *s = new folia::Sentence( args, e->doc() );
	  append_to_sentence( s, sents[0] );
	  ++sentence_done;
	  if  (tokDebug > 0){
	    LOG << "created a new sentence: " << s << endl;
	  }
	  e->append( s );
	}
      }
      else if ( !pv.empty() ){
	// For now we only handle the Paragraphs, ignore sentences and words
	// IS this even valid???
	for ( const auto& p : pv ){
	  handle_one_paragraph( p, sentence_done );
	}
      }
      else {
	// For now we just IGNORE the loose words (backward compatability)
	for ( const auto& s : sv ){
	  handle_one_sentence( s, sentence_done );
	}
      }
    }
    if ( text_redundancy == "full" ){
      appendText( e, outputclass );
    }
    else if ( text_redundancy == "none" ){
      removeText( e, outputclass );
    }
  }

  folia::Document *TokenizerClass::tokenize_folia( const string& infile_name ){
    if ( inputclass == outputclass ){
      LOG << "ucto: --filter=NO is automatically set. inputclass equals outputclass!"
	  << endl;
      setFiltering(false);
    }
    folia::TextProcessor proc( infile_name );
    if ( passthru ){
      proc.declare( folia::AnnotationType::TOKEN, "passthru",
		    "annotator='ucto', annotatortype='auto', datetime='now()'" );
    }
    else {
      if ( !proc.is_declared( folia::AnnotationType::LANG ) ){
	proc.declare( folia::AnnotationType::LANG,
		      ISO_SET, "annotator='ucto'" );
      }
      if ( proc.doc()->metadatatype() == "native"
	   && default_language != "none" ){
	proc.set_metadata( "language", default_language );
      }
      else {
	LOG << "[WARNING] cannot set meta data language on FoLiA documents of type: "
	    << proc.doc()->metadatatype() << endl;
      }
      proc.declare( folia::AnnotationType::TOKEN,
		    "tokconfig-nld",
		    "annotator='ucto', annotatortype='auto', datetime='now()'");
    }
    if  ( tokDebug > 8){
      proc.set_dbg_stream( theErrLog );
      proc.set_debug( true );
    }
    //    proc.set_debug( true );
    proc.setup( inputclass, true );
    int sentence_done = 0;
    folia::FoliaElement *p = 0;
    while ( (p = proc.next_text_parent() ) ){
      //    LOG << "next text parent: " << p << endl;
      handle_one_text_parent( p, sentence_done );
      if ( tokDebug > 0 ){
	LOG << "done with sentence " << sentence_done << endl;
      }
      if ( proc.next() ){
	if ( tokDebug > 1 ){
	  LOG << "looping for more ..." << endl;
	}
      }
    }
    if ( sentence_done == 0 ){
      LOG << "document contains no text in the desired inputclass: "
	  << inputclass << endl;
      LOG << "NO result!" << endl;
      return 0;
    }
    return proc.doc(true); // take the doc over from the processor
  }

  void TokenizerClass::tokenize_folia( const string& infile_name,
				       const string& outfile_name ){
    if ( tokDebug > 0 ){
      LOG << "[tokenize_folia] (" << infile_name << ","
	  << outfile_name << ")" << endl;
    }
    folia::Document *doc = tokenize_folia( infile_name );
    if ( doc ){
      doc->save( outfile_name, false );
      if ( tokDebug > 0 ){
	LOG << "resulting FoLiA doc saved in " << outfile_name << endl;
      }
    }
    else {
      if ( tokDebug > 0 ){
	LOG << "NO FoLiA doc created! " << endl;
      }
    }
  }

  void TokenizerClass::outputTokens( ostream& OUT,
				     const vector<Token>& tokens,
				     const bool continued ) const {
    // continued should be set to true when outputTokens is invoked multiple
    // times and it is not the first invokation
    // this makes paragraph boundaries work over multiple calls
    short quotelevel = 0;
    bool first = true;
    for ( const auto token : tokens ) {
      if (tokDebug >= 5){
	LOG << "outputTokens: token=" << token << endl;
      }
      if ( detectPar
	   && (token.role & NEWPARAGRAPH)
	   && !verbose
	   && ( !first || continued ) ) {
	//output paragraph separator
	if (sentenceperlineoutput) {
	  OUT << endl;
	}
	else {
	  OUT << endl << endl;
	}
      }
      UnicodeString s = token.us;
      if (lowercase) {
	s = s.toLower();
      }
      else if (uppercase) {
	s = s.toUpper();
      }
      OUT << s;
      if ( token.role & NEWPARAGRAPH) {
	quotelevel = 0;
      }
      if ( token.role & BEGINQUOTE) {
	++quotelevel;
      }
      if (verbose) {
	OUT << "\t" << token.type << "\t" << token.role << endl;
      }
      if ( token.role & ENDQUOTE) {
	--quotelevel;
      }

      if ( token.role & ENDOFSENTENCE) {
	if ( verbose ) {
	  if ( !(token.role & NOSPACE ) ){
	    OUT << endl;
	  }
	}
	else {
	  if ( quotelevel == 0 ) {
	    if (sentenceperlineoutput) {
	      OUT << endl;
	    }
	    else {
	      OUT << " " + eosmark + " ";
	    }
	    if ( splitOnly ){
	      OUT << endl;
	    }
	  }
	  else { //inside quotation
	    if ( splitOnly
		 && !(token.role & NOSPACE ) ){
	      OUT << " ";
	    }
	  }
	}
      }
      if ( ( &token != &(*tokens.rbegin()) )
	   && !verbose ) {
	if ( !( (token.role & ENDOFSENTENCE)
		&& sentenceperlineoutput
		&& !splitOnly ) ){
	  if ( !(token.role & ENDOFSENTENCE) ){
	    if ( splitOnly
		 && (token.role & NOSPACE) ){
	    }
	    else {
	      OUT << " ";
	    }
	  }
	}
	else if ( (quotelevel > 0)
		  && sentenceperlineoutput ) {
	  //FBK: ADD SPACE WITHIN QUOTE CONTEXT IN ANY CASE
	  OUT << " ";
	}
      }
    }
  }

  int TokenizerClass::countSentences( bool forceentirebuffer ) {
    //Return the number of *completed* sentences in the token buffer

    //Performs  extra sanity checks at the same time! Making sure
    //BEGINOFSENTENCE and ENDOFSENTENCE always pair up, and that TEMPENDOFSENTENCE roles
    //are converted to proper ENDOFSENTENCE markers

    short quotelevel = 0;
    int count = 0;
    const int size = tokens.size();
    int begin = 0;
    int i = 0;
    for ( auto& token : tokens ) {
      if (tokDebug >= 5){
	LOG << "[countSentences] buffer#" <<i
			<< " word=[" << token.us
			<< "] role=" << token.role
			<< ", quotelevel="<< quotelevel << endl;
      }
      if (token.role & NEWPARAGRAPH) quotelevel = 0;
      if (token.role & BEGINQUOTE) quotelevel++;
      if (token.role & ENDQUOTE) quotelevel--;
      if ( forceentirebuffer
	   && (token.role & TEMPENDOFSENTENCE)
	   && (quotelevel == 0)) {
	//we thought we were in a quote, but we're not... No end quote was found and an end is forced now.
	//Change TEMPENDOFSENTENCE to ENDOFSENTENCE and make sure sentences match up sanely
	token.role &= ~TEMPENDOFSENTENCE;
	token.role |= ENDOFSENTENCE;
      }
      tokens[begin].role |= BEGINOFSENTENCE;  //sanity check
      if ( (token.role & ENDOFSENTENCE)
	   && (quotelevel == 0) ) {
	begin = i + 1;
	count++;
	if (tokDebug >= 5){
	  LOG << "[countSentences] SENTENCE #" << count << " found" << endl;
	}
      }
      if ( forceentirebuffer
	   && ( i == size - 1)
	   && !(token.role & ENDOFSENTENCE) )  {
	//last token of buffer
	count++;
	token.role |= ENDOFSENTENCE;
	if (tokDebug >= 5){
	  LOG << "[countSentences] SENTENCE #" << count << " *FORCIBLY* ended" << endl;
	}
      }
      ++i;
    }
    return count;
  }

  vector<Token> TokenizerClass::popSentence( ) {
    vector<Token> outToks;
    const int size = tokens.size();
    if ( size != 0 ){
      short quotelevel = 0;
      size_t begin = 0;
      size_t end = 0;
      for ( int i = 0; i < size; ++i ) {
	if (tokens[i].role & NEWPARAGRAPH) {
	  quotelevel = 0;
	}
	else if (tokens[i].role & ENDQUOTE) {
	  --quotelevel;
	}
	if ( (tokens[i].role & BEGINOFSENTENCE)
	     && (quotelevel == 0)) {
	  begin = i;
	}
	//FBK: QUOTELEVEL GOES UP BEFORE begin IS UPDATED... RESULTS IN DUPLICATE OUTPUT
	if (tokens[i].role & BEGINQUOTE) {
	  ++quotelevel;
	}

	if ((tokens[i].role & ENDOFSENTENCE) && (quotelevel == 0)) {
	  end = i;
	  if (tokDebug >= 1){
	    LOG << "[tokenize] extracted sentence, begin=" << begin
		<< ",end="<< end << endl;
	  }
	  for ( size_t i=begin; i <= end; ++i ){
	    outToks.push_back( tokens[i] );
	  }
	  tokens.erase( tokens.begin(), tokens.begin()+end+1 );
	  if ( !passthru ){
	    string lang = get_language( outToks );
	    if ( !settings[lang]->quotes.emptyStack() ) {
	      settings[lang]->quotes.flushStack( end+1 );
	    }
	  }
	  // we are done...
	  return outToks;
	}
      }
    }
    return outToks;
  }

  string TokenizerClass::getString( const vector<Token>& v ){
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
    int numS = countSentences(true); // force buffer to end with END_OF_SENTENCE
    for (int i = 0; i < numS; i++) {
      vector<Token> v = popSentence( );
      string tmp = getString( v );
      sentences.push_back( tmp );
    }
    return sentences;
  }

  // FBK: return true if character is a quote.
  bool TokenizerClass::u_isquote( UChar32 c, const Quoting& quotes ) const {
    bool quote = false;
    if ( u_hasBinaryProperty( c, UCHAR_QUOTATION_MARK )
	 || c == '`'
	 || c == U'´' ) {
      // M$ users use the spacing grave and acute accents often as a
      // quote (apostroph) but is DOESN`T have the UCHAR_QUOTATION_MARK property
      // so trick that
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

  bool TokenizerClass::resolveQuote( int endindex,
				     const UnicodeString& open,
				     Quoting& quotes ) {
    //resolve a quote
    int stackindex = -1;
    int beginindex = quotes.lookup( open, stackindex );

    if (beginindex >= 0) {
      if (tokDebug >= 2) {
	LOG << "[resolveQuote] Quote found, begin="<< beginindex << ", end="<< endindex << endl;
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
	    tokens[i].role &= ~TEMPENDOFSENTENCE;
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
	if ( tokDebug >= 2 ) {
	  LOG << "marked BEGIN: " << tokens[beginindex] << endl;
	  LOG << "marked   END: " << tokens[endindex] << endl;
	}
      }
      else if ( expectingend == 1
		&& subquote == 0
		&& !( tokens[endindex - 1].role & ENDOFSENTENCE) ) {
	//missing one endofsentence, we can correct, last token in quote token is endofsentence:
	if ( tokDebug >= 2 ) {
	  LOG << "[resolveQuote] Missing endofsentence in quote, fixing... " << expectingend << endl;
	}
	tokens[endindex - 1].role |= ENDOFSENTENCE;
	//mark the quote
	tokens[beginindex].role |= BEGINQUOTE;
	tokens[endindex].role |= ENDQUOTE;
      }
      else {
	if ( tokDebug >= 2) {
	  LOG << "[resolveQuote] Quote can not be resolved, unbalanced sentences or subquotes within quote, skipping... (expectingend=" << expectingend << ",subquote=" << subquote << ")" << endl;
	}
	//something is wrong. Sentences within quote are not balanced, so we won't mark the quote.
      }
      //remove from stack (ok, granted, stack is a bit of a misnomer here)
      quotes.eraseAtPos( stackindex );
      //FBK: ENDQUOTES NEED TO BE MARKED AS ENDOFSENTENCE IF THE PREVIOUS TOKEN
      //WAS AN ENDOFSENTENCE. OTHERWISE THE SENTENCES WILL NOT BE SPLIT.
      if ( tokens[endindex].role & ENDQUOTE
	   && tokens[endindex-1].role & ENDOFSENTENCE ) {
        //FBK: CHECK FOR EOS AFTER QUOTES
        if ((endindex+1 == size) || //FBK: endindex EQUALS TOKEN SIZE, MUST BE EOSMARKERS
            ((endindex + 1 < size) && (is_BOS(tokens[endindex+1].us[0])))) {
	  tokens[endindex].role |= ENDOFSENTENCE;
	  // FBK: CHECK IF NEXT TOKEN IS A QUOTE AND NEXT TO THE QUOTE A BOS
        }
	else if ( endindex + 2 < size
		  && u_isquote( tokens[endindex+1].us[0], quotes )
		  && is_BOS( tokens[endindex+2].us[0] ) ) {
	  tokens[endindex].role |= ENDOFSENTENCE;
	  // If the current token is an ENDQUOTE and the next token is a quote and also the last token,
	  // the current token is an EOS.
        }
	else if ( endindex + 2 == size
		  && u_isquote( tokens[endindex+1].us[0], quotes ) ) {
	  tokens[endindex].role |= ENDOFSENTENCE;
        }
      }
      return true;
    }
    else {
      return false;
    }
  }

  bool TokenizerClass::detectEos( size_t i,
				  const UnicodeString& eosmarkers,
				  const Quoting& quotes ) const {
    bool is_eos = false;
    UChar32 c = tokens[i].us.char32At(0);
    if ( c == '.' || eosmarkers.indexOf( c ) >= 0 ){
      if (i + 1 == tokens.size() ) {	//No next character?
	is_eos = true; //Newline after eosmarker
      }
      else {
	UChar32 c = tokens[i+1].us.char32At(0);
	if ( u_isquote( c, quotes ) ){
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

  void TokenizerClass::detectQuoteBounds( const int i,
					  Quoting& quotes ) {
    UChar32 c = tokens[i].us.char32At(0);
    //Detect Quotation marks
    if ((c == '"') || ( UnicodeString(c) == "＂") ) {
      if (tokDebug > 1 ){
	LOG << "[detectQuoteBounds] Standard double-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c,quotes)) {
	if (tokDebug > 1 ) {
	  LOG << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else if ( c == '\'' ) {
      if (tokDebug > 1 ){
	LOG << "[detectQuoteBounds] Standard single-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c,quotes)) {
	if (tokDebug > 1 ) {
	  LOG << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else {
      UnicodeString close = quotes.lookupOpen( c );
      if ( !close.isEmpty() ){ // we have a opening quote
	if ( tokDebug > 1 ) {
	  LOG << "[detectQuoteBounds] Opening quote found @i="<< i << ", pushing to stack for resolution later..." << endl;
	}
	quotes.push( i, c ); // remember it
      }
      else {
	UnicodeString open = quotes.lookupClose( c );
	if ( !open.isEmpty() ) { // we have a closing quote
	  if (tokDebug > 1 ) {
	    LOG << "[detectQuoteBounds] Closing quote found @i="<< i << ", attempting to resolve..." << endl;
	  }
	  if ( !resolveQuote( i, open, quotes )) {
	    // resolve the matching opening
	    if (tokDebug > 1 ) {
	      LOG << "[detectQuoteBounds] Unable to resolve" << endl;
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

  void TokenizerClass::detectSentenceBounds( const int offset,
					     const string& lang ){
    //find sentences
    string method;
    if ( detectQuotes ){
      method = "[detectSentenceBounds-(quoted)]";
    }
    else {
      method = "[detectSentenceBounds]";
    }
    const int size = tokens.size();
    for (int i = offset; i < size; i++) {
      if (tokDebug > 1 ){
	LOG << method << " i="<< i << " word=[" << tokens[i].us
	    << "] type=" << tokens[i].type
	    << ", role=" << tokens[i].role << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ){
	if ((tokDebug > 1 )){
	  LOG << method << " PUNCTUATION FOUND @i=" << i << endl;
	}
	// we have some kind of punctuation. Does it mark an eos?
	bool is_eos = detectEos( i,
				 settings[lang]->eosmarkers,
				 settings[lang]->quotes );
	if (is_eos) {
	  // end of sentence found/ so wrap up
	  if ( detectQuotes
	       && !settings[lang]->quotes.emptyStack() ) {
	    // we have some quotes!
	    if ( tokDebug > 1 ){
	      LOG << method << " Unbalances quotes: Preliminary EOS FOUND @i="
		  << i << endl;
	    }
	    // we set a temporary EOS marker,
	    // to be resolved later when full quote is found.
	    tokens[i].role |= TEMPENDOFSENTENCE;
	    // If previous token is also TEMPENDOFSENTENCE,
	    // it stops being so in favour of this one
	    if ( i > 0 ){
	      tokens[i-1].role &= ~TEMPENDOFSENTENCE;
	    }
	  }
	  else {
	    // No quotes
	    if ( tokDebug > 1 ){
	      LOG << method << " EOS FOUND @i=" << i << endl;
	    }
	    tokens[i].role |= ENDOFSENTENCE;
	    // if this is the end of the sentence,
	    // the next token is the beginning of a new one
	    if ( (i + 1) < size ){
	      tokens[i+1].role |= BEGINOFSENTENCE;
	    }
	    // if previous token is EOS and not BOS, it will stop being EOS,
	    // as this one will take its place
	    if ( i > 0
		 && ( tokens[i-1].role & ENDOFSENTENCE )
		 && !( tokens[i-1].role & BEGINOFSENTENCE ) ) {
	      tokens[i-1].role &= ~ENDOFSENTENCE;
	      tokens[i].role &= ~BEGINOFSENTENCE;
	    }
	  }
	}
	else if ( isClosing(tokens[i] ) ) {
	  // we have a closing symbol
	  if ( tokDebug > 1 ){
	    LOG << method << " Close FOUND @i=" << i << endl;
	  }
	  //if previous token is EOS and not BOS, it will stop being EOS, as this one will take its place
	  if ( i > 0
	       && ( tokens[i-1].role & ENDOFSENTENCE )
	       && !( tokens[i-1].role & BEGINOFSENTENCE) ) {
	    tokens[i-1].role &= ~ENDOFSENTENCE;
	    tokens[i].role &= ~BEGINOFSENTENCE;
	  }
	}
	if ( detectQuotes ){
	  // check the quotes
	  detectQuoteBounds( i, settings[lang]->quotes );
	}
      }
    }
    for (int i = size-1; i > offset; --i ) {
      // at the end of the buffer there may be some PUNCTUATION which
      // has spurious ENDOFSENTENCE and BEGINOFSENTENCE annotation
      // fix this up to avoid sentences containing only punctuation
      // also we don't want a BEGINQUOTE to be an ENDOFSENTENCE
      if ( tokDebug > 2 ){
	LOG << method << " fixup-end i="<< i << " word=["
	    << tokens[i].us
	    << "] type=" << tokens[i].type
	    << ", role=" << tokens[i].role << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ) {
	tokens[i].role &= ~BEGINOFSENTENCE;
	if ( !detectQuotes ||
	     (tokens[i].role & BEGINQUOTE) ){
	  if ( i != size-1 ){
	    tokens[i].role &= ~ENDOFSENTENCE;
	  }
	}
      }
      else
	break;
    }
  }

  void TokenizerClass::passthruLine( const UnicodeString& input, bool& bos ) {
    if (tokDebug) {
      LOG << "[passthruLine] input: line=[" << input << "]" << endl;
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
	  LOG << "[passthruLine] word=[" << word << "]" << endl;
	}
	if ( word == eosmark ) {
	  word = "";
	  if (!tokens.empty())
	    tokens.back().role |= ENDOFSENTENCE;
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
		    type == type_emoticon || type == type_picto ) ) {
	    if (tokDebug >= 2 ){
	      LOG << "   [passThruLine] skipped PUNCTUATION ["
			      << input << "]" << endl;
	    }
	    if ( !tokens.empty() ){
	      tokens.back().role &= ~NOSPACE;
	    }
	  }
	  else {
	    if ( norm_set.find( type ) != norm_set.end() ){
	      word = "{{" + type + "}}";
	    }
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
	  tokens.back().role |= ENDOFSENTENCE;
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
		  type == type_emoticon || type == type_picto ) ) {
	  if (tokDebug >= 2 ){
	    LOG << "   [passThruLine] skipped PUNCTUATION ["
			    << input << "]" << endl;
	  }
	  if ( !tokens.empty() ){
	    tokens.back().role &= ~NOSPACE;
	  }
	}
	else {
	  if ( norm_set.find( type ) != norm_set.end() ){
	    word = "{{" + type + "}}";
	  }
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
    if ( sentenceperlineinput && tokens.size() > 0 ) {
      tokens[0].role |= BEGINOFSENTENCE;
      tokens.back().role |= ENDOFSENTENCE;
    }
  }

  string TokenizerClass::checkBOM( istream& in ){
    string result = inputEncoding;
    if ( &in == &cin ){
      return result;
    }
    streampos pos = in.tellg();
    string s;
    in >> s;
    UErrorCode err = U_ZERO_ERROR;
    int32_t bomLength = 0;
    const char *encoding = ucnv_detectUnicodeSignature( s.c_str(), s.length(),
							&bomLength, &err);
    if ( bomLength ){
      if ( tokDebug ){
	LOG << "Autodetected encoding: " << encoding << endl;
      }
      result = encoding;
      if ( result == "UTF16BE"
	   || result == "UTF-16BE" ){
	result = "UTF16BE";
      }
    }
    in.seekg( pos + (streampos)bomLength );
    return result;
  }

  // string wrapper
  void TokenizerClass::tokenizeLine( const string& s ){
    UnicodeString us = convert( s, inputEncoding );
    tokenizeLine( us );
  }

  // UnicodeString wrapper
  void TokenizerClass::tokenizeLine( const UnicodeString& us ){
    bool bos = true;
    tokenize_one_line( us, bos );
    countSentences(true); // force the ENDOFSENTENCE
  }

  bool u_isemo( UChar32 c ){
    UBlockCode s = ublock_getCode(c);
    return s == UBLOCK_EMOTICONS;
  }

  bool u_ispicto( UChar32 c ){
    UBlockCode s = ublock_getCode(c);
    return s == UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS ;
  }

  bool u_iscurrency( UChar32 c ){
    return u_charType( c ) == U_CURRENCY_SYMBOL;
  }

  bool u_issymbol( UChar32 c ){
    return u_charType( c ) == U_CURRENCY_SYMBOL
      || u_charType( c ) == U_MATH_SYMBOL
      || u_charType( c ) == U_MODIFIER_SYMBOL
      || u_charType( c ) == U_OTHER_SYMBOL;
  }

  const UnicodeString& detect_type( UChar32 c ){
    if ( u_isspace(c)) {
      return type_space;
    }
    else if ( u_iscurrency(c)) {
      return type_currency;
    }
    else if ( u_ispunct(c)) {
      return type_punctuation;
    }
    else if ( u_isemo( c ) ) {
      return type_emoticon;
    }
    else if ( u_ispicto( c ) ) {
      return type_picto;
    }
    else if ( u_isalpha(c)) {
      return type_word;
    }
    else if ( u_isdigit(c)) {
      return type_number;
    }
    else if ( u_issymbol(c)) {
      return type_symbol;
    }
    else {
      return type_unknown;
    }
  }

  std::string toString( int8_t c ){
    switch ( c ){
    case 0:
      return "U_UNASSIGNED";
    case 1:
      return "U_UPPERCASE_LETTER";
    case 2:
      return "U_LOWERCASE_LETTER";
    case 3:
      return "U_TITLECASE_LETTER";
    case 4:
      return "U_MODIFIER_LETTER";
    case 5:
      return "U_OTHER_LETTER";
    case 6:
      return "U_NON_SPACING_MARK";
    case 7:
      return "U_ENCLOSING_MARK";
    case 8:
      return "U_COMBINING_SPACING_MARK";
    case 9:
      return "U_DECIMAL_DIGIT_NUMBER";
    case 10:
      return "U_LETTER_NUMBER";
    case 11:
      return "U_OTHER_NUMBER";
    case 12:
      return "U_SPACE_SEPARATOR";
    case 13:
      return "U_LINE_SEPARATOR";
    case 14:
      return "U_PARAGRAPH_SEPARATOR";
    case 15:
      return "U_CONTROL_CHAR";
    case 16:
      return "U_FORMAT_CHAR";
    case 17:
      return "U_PRIVATE_USE_CHAR";
    case 18:
      return "U_SURROGATE";
    case 19:
      return "U_DASH_PUNCTUATION";
    case 20:
      return "U_START_PUNCTUATION";
    case 21:
      return "U_END_PUNCTUATION";
    case 22:
      return "U_CONNECTOR_PUNCTUATION";
    case 23:
      return "U_OTHER_PUNCTUATION";
    case 24:
      return "U_MATH_SYMBOL";
    case 25:
      return "U_CURRENCY_SYMBOL";
    case 26:
      return "U_MODIFIER_SYMBOL";
    case 27:
      return "U_OTHER_SYMBOL";
    case 28:
      return "U_INITIAL_PUNCTUATION";
    case 29:
      return "U_FINAL_PUNCTUATION";
    default:
      return "OMG NO CLUE WHAT KIND OF SYMBOL THIS IS: "
	+ TiCC::toString( int(c) );
    }
  }

  int TokenizerClass::tokenizeLine( const UnicodeString& originput,
				    const string& _lang ){
    string lang = _lang;
    if ( lang.empty() ){
      lang = "default";
    }
    else {
      auto const it = settings.find( lang );
      if ( it == settings.end() ){
	LOG << "tokenizeLine: no settings found for language=" + lang << endl
	    << "using the default language instead:" << default_language << endl;
	lang = "default";
      }
    }
    if (tokDebug){
      LOG << "[tokenizeLine] input: line=["
	  << originput << "] (" << lang << ")" << endl;
    }
    UnicodeString input = normalizer.normalize( originput );
    if ( doFilter ){
      input = settings[lang]->filter.filter( input );
    }
    if ( input.isBogus() ){ //only tokenize valid input
      LOG << "ERROR: Invalid UTF-8 in line:" << linenum << endl
	  << "   '" << input << "'" << endl;
      return 0;
    }
    int32_t len = input.countChar32();
    if (tokDebug){
      LOG << "[tokenizeLine] filtered input: line=["
		      << input << "] (" << len
		      << " unicode characters)" << endl;
    }
    const int begintokencount = tokens.size();
    if (tokDebug) {
      LOG << "[tokenizeLine] Tokens still in buffer: " << begintokencount << endl;
    }

    bool tokenizeword = false;
    bool reset = false;
    //iterate over all characters
    UnicodeString word;
    StringCharacterIterator sit(input);
    long int i = 0;
    long int tok_size = 0;
    while ( sit.hasNext() ){
      UChar32 c = sit.current32();
      if ( tokDebug > 8 ){
	UnicodeString s = c;
	int8_t charT = u_charType( c );
	LOG << "examine character: " << s << " type= "
	    << toString( charT  ) << endl;
      }
      if (reset) { //reset values for new word
	reset = false;
	tok_size = 0;
	if (!u_isspace(c))
	  word = c;
	else
	  word = "";
	tokenizeword = false;
      }
      else {
	if ( !u_isspace(c) ){
	  word += c;
	}
      }
      if ( u_isspace(c) || i == len-1 ){
	if (tokDebug){
	  LOG << "[tokenizeLine] space detected, word=["
			  << word << "]" << endl;
	}
	if ( i == len-1 ) {
	  if ( u_ispunct(c)
	       || u_isdigit(c)
	       || u_isquote( c, settings[lang]->quotes )
	       || u_isemo(c) ){
	    tokenizeword = true;
	  }
	}
	int expliciteosfound = -1;
	if ( word.length() >= eosmark.length() ) {
	  expliciteosfound = word.lastIndexOf(eosmark);

	  if (expliciteosfound != -1) { // word contains eosmark
	    if ( tokDebug >= 2){
	      LOG << "[tokenizeLine] Found explicit EOS marker @"<<expliciteosfound << endl;
	    }
	    int eospos = tokens.size()-1;
	    if (expliciteosfound > 0) {
	      UnicodeString realword;
	      word.extract(0,expliciteosfound,realword);
	      if (tokDebug >= 2) {
		LOG << "[tokenizeLine] Prefix before EOS: "
				<< realword << endl;
	      }
	      tokenizeWord( realword, false, lang );
	      eospos++;
	    }
	    if ( expliciteosfound + eosmark.length() < word.length() ){
	      UnicodeString realword;
	      word.extract( expliciteosfound+eosmark.length(),
			    word.length() - expliciteosfound - eosmark.length(),
			    realword );
	      if (tokDebug >= 2){
		LOG << "[tokenizeLine] postfix after EOS: "
				<< realword << endl;
	      }
	      tokenizeWord( realword, true, lang );
	    }
	    if ( !tokens.empty() && eospos >= 0 ) {
	      if (tokDebug >= 2){
		LOG << "[tokenizeLine] Assigned EOS" << endl;
	      }
	      tokens[eospos].role |= ENDOFSENTENCE;
	    }
	  }
	}
	if ( word.length() > 0
	     && expliciteosfound == -1 ) {
	  if (tokDebug >= 2){
	    LOG << "[tokenizeLine] Further tokenisation necessary for: ["
			    << word << "]" << endl;
	  }
	  if ( tokenizeword ) {
	    tokenizeWord( word, true, lang );
	  }
	  else {
	    tokenizeWord( word, true, lang, type_word );
	  }
	}
	//reset values for new word
	reset = true;
      }
      else if ( u_ispunct(c)
		|| u_isdigit(c)
		|| u_isquote( c, settings[lang]->quotes )
		|| u_isemo(c) ){
	if (tokDebug){
	  LOG << "[tokenizeLine] punctuation or digit detected, word=["
			  << word << "]" << endl;
	}
	//there is punctuation or digits in this word, mark to run through tokeniser
	tokenizeword = true;
      }
      sit.next32();
      ++i;
      ++tok_size;
      if ( tok_size > 2500 ){
	LOG << "Ridiculously long word/token (over 2500 characters) detected "
	    << "in line: " << linenum << ". Skipped ..." << endl;
	LOG << "The line starts with " << UnicodeString( word, 0, 75 )
	    << "..." << endl;
	return 0;
      }
    }
    int numNewTokens = tokens.size() - begintokencount;
    if ( numNewTokens > 0 ){
      if (paragraphsignal) {
	tokens[begintokencount].role |= NEWPARAGRAPH | BEGINOFSENTENCE;
	paragraphsignal = false;
      }
      //find sentence boundaries
      if (sentenceperlineinput) {
	// force it to be a sentence
	tokens[begintokencount].role |= BEGINOFSENTENCE;
	tokens.back().role |= ENDOFSENTENCE;
      }
      detectSentenceBounds( begintokencount );
    }
    return numNewTokens;
  }

  void TokenizerClass::tokenizeWord( const UnicodeString& input,
				     bool space,
				     const string& lang,
				     const UnicodeString& assigned_type ) {
    bool recurse = !assigned_type.isEmpty();

    int32_t inpLen = input.countChar32();
    if ( tokDebug > 2 ){
      if ( recurse ){
	LOG << "   [tokenizeWord] Recurse Input: (" << inpLen << ") "
	    << "word=[" << input << "], type=" << assigned_type
	    << " Space=" << (space?"TRUE":"FALSE") << endl;
      }
      else {
	LOG << "   [tokenizeWord] Input: (" << inpLen << ") "
	    << "word=[" << input << "]"
	    << " Space=" << (space?"TRUE":"FALSE") << endl;      }
    }
    if ( input == eosmark ) {
      if (tokDebug >= 2){
	LOG << "   [tokenizeWord] Found explicit EOS marker" << endl;
      }
      if (!tokens.empty()) {
	if (tokDebug >= 2){
	  LOG << "   [tokenizeWord] Assigned EOS" << endl;
	}
	tokens.back().role |= ENDOFSENTENCE;
      }
      else {
	LOG << "[WARNING] Found explicit EOS marker by itself, this will have no effect!" << endl;
      }
      return;
    }

    if ( inpLen == 1) {
      //single character, no need to process all rules, do some simpler (faster) detection
      UChar32 c = input.char32At(0);
      UnicodeString type = detect_type( c );
      if ( type == type_space ){
	return;
      }
      if ( doPunctFilter
	   && ( type == type_punctuation || type == type_currency ||
		type == type_emoticon || type == type_picto ) ) {
	if (tokDebug >= 2 ){
	  LOG << "   [tokenizeWord] skipped PUNCTUATION ["
			  << input << "]" << endl;
	}
	if ( !tokens.empty() ){
	  tokens.back().role &= ~NOSPACE;
	}
      }
      else {
	UnicodeString word = input;
	if ( norm_set.find( type ) != norm_set.end() ){
	  word = "{{" + type + "}}";
	}
	Token T( type, word, space ? NOROLE : NOSPACE, lang );
	tokens.push_back( T );
	if (tokDebug >= 2){
	  LOG << "   [tokenizeWord] added token " << T << endl;
	}
      }
    }
    else {
      bool a_rule_matched = false;
      for ( const auto& rule : settings[lang]->rules ) {
	if ( tokDebug >= 4){
	  LOG << "\tTESTING " << rule->id << endl;
	}
	UnicodeString type = rule->id;
	//Find first matching rule
	UnicodeString pre, post;
	vector<UnicodeString> matches;
	if ( rule->matchAll( input, pre, post, matches ) ){
	  a_rule_matched = true;
	  if ( tokDebug >= 4 ){
	    LOG << "\tMATCH: " << type << endl;
	    LOG << "\tpre=  '" << pre << "'" << endl;
	    LOG << "\tpost= '" << post << "'" << endl;
	    int cnt = 0;
	    for ( const auto& m : matches ){
	      LOG << "\tmatch[" << ++cnt << "]=" << m << endl;
	    }
	  }
	  if ( recurse
	       && ( type == type_word
		    || ( pre.isEmpty()
			 && post.isEmpty() ) ) ){
	    // so only do this recurse step when:
	    //   OR we have a WORD
	    //   OR we have an exact match of the rule (no pre or post)
	    if ( assigned_type != type_word ){
	      // don't change the type when:
	      //   it was already non-WORD
	      if ( tokDebug >= 4 ){
		LOG << "\trecurse, match didn't do anything new for " << input << endl;
	      }
	      tokens.push_back( Token( assigned_type, input, space ? NOROLE : NOSPACE, lang ) );
	      return;
	    }
	    else {
	      if ( tokDebug >= 4 ){
		LOG << "\trecurse, match changes the type:"
				<< assigned_type << " to " << type << endl;
	      }
	      tokens.push_back( Token( type, input, space ? NOROLE : NOSPACE, lang ) );
	      return;
	    }
	  }
	  if ( pre.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      LOG << "\tTOKEN pre-context (" << pre.length()
			      << "): [" << pre << "]" << endl;
	    }
	    tokenizeWord( pre, false, lang ); //pre-context, no space after
	  }
	  if ( matches.size() > 0 ){
	    int max = matches.size();
	    if ( tokDebug >= 4 ){
	      LOG << "\tTOKEN match #=" << matches.size() << endl;
	    }
	    for ( int m=0; m < max; ++m ){
	      if ( tokDebug >= 4 ){
		LOG << "\tTOKEN match[" << m << "] = " << matches[m]
		    << " Space=" << (space?"TRUE":"FALSE") << endl;
	      }
	      if ( doPunctFilter
		   && (&rule->id)->startsWith("PUNCTUATION") ){
		if (tokDebug >= 2 ){
		  LOG << "   [tokenizeWord] skipped PUNCTUATION ["
				  << matches[m] << "]" << endl;
		}
		if ( !tokens.empty() ){
		  tokens.back().role &= ~NOSPACE;
		}
	      }
	      else {
		bool internal_space = space;
		if ( post.length() > 0 ) {
		  internal_space = false;
		}
		else if ( m < max-1 ){
		  internal_space = false;
		}
		UnicodeString word = matches[m];
		if ( norm_set.find( type ) != norm_set.end() ){
		  word = "{{" + type + "}}";
		  tokens.push_back( Token( type, word, internal_space ? NOROLE : NOSPACE, lang ) );
		}
		else {
		  if ( recurse ){
		    tokens.push_back( Token( type, word, internal_space ? NOROLE : NOSPACE, lang ) );
		  }
		  else {
		    tokenizeWord( word, internal_space, lang, type );
		  }
		}
	      }
	    }
	  }
	  else if ( tokDebug >=4 ){
	    // should never come here?
	    LOG << "\tPANIC there's no match" << endl;
	  }
	  if ( post.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      LOG << "\tTOKEN post-context (" << post.length()
			      << "): [" << post << "]" << endl;
	    }
	    tokenizeWord( post, space, lang );
	  }
	  break;
	}
      }
      if ( !a_rule_matched ){
	// no rule matched
	if ( tokDebug >=4 ){
	  LOG << "\tthere's no match at all" << endl;
	}
	tokens.push_back( Token( assigned_type, input, space ? NOROLE : NOSPACE , lang ) );
      }
    }
  }

  bool TokenizerClass::init( const string& fname, const string& tname ){
    if ( tokDebug ){
      LOG << "Initiating tokeniser..." << endl;
    }
    Setting *set = new Setting();
    if ( !set->read( fname, tname, tokDebug, theErrLog ) ){
      LOG << "Cannot read Tokeniser settingsfile " << fname << endl;
      LOG << "Unsupported language? (Did you install the uctodata package?)"
	  << endl;
      return false;
    }
    else {
      settings["default"] = set;
      default_language = "default";
    }
    if ( tokDebug ){
      LOG << "effective rules: " << endl;
      for ( size_t i=0; i < set->rules.size(); ++i ){
	LOG << "rule " << i << " " << *(set->rules[i]) << endl;
      }
      LOG << "EOS markers: " << set->eosmarkers << endl;
      LOG << "Quotations: " << set->quotes << endl;
      try {
	LOG << "Filter: " << set->filter << endl;
      }
      catch (...){
      }
    }
    return true;
  }

  bool TokenizerClass::init( const vector<string>& languages,
			     const string& tname ){
    if ( tokDebug > 0 ){
      LOG << "Initiating tokeniser from language list..." << endl;
    }
    Setting *default_set = 0;
    for ( const auto& lang : languages ){
      if ( tokDebug > 0 ){
	LOG << "init language=" << lang << endl;
      }
      string fname = "tokconfig-" + lang;
      Setting *set = new Setting();
      string add;
      if ( default_set == 0 ){
	add = tname;
      }
      if ( !set->read( fname, add, tokDebug, theErrLog ) ){
	LOG << "problem reading datafile for language: " << lang << endl;
	LOG << "Unsupported language (Did you install the uctodata package?)"
	    << endl;
      }
      else {
	if ( default_set == 0 ){
	  default_set = set;
	  settings["default"] = set;
	  default_language = lang;
	}
	settings[lang] = set;
      }
    }
    if ( settings.empty() ){
      cerr << "ucto: No useful settingsfile(s) could be found." << endl;
      return false;
    }
    return true;
  }

  string get_language( const vector<Token>& tv ){
    string result = "default";
    for ( const auto& t : tv ){
      if ( !t.lang_code.empty() && t.lang_code != "default" ){
	if ( result == "default" ){
	  result = t.lang_code;
	}
	if ( result != t.lang_code ){
	  throw logic_error( "ucto: conflicting language(s) assigned" );
	}
      }
    }
    return result;
  }

} //namespace Tokenizer
