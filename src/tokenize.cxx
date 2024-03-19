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

#include "ucto/tokenize.h"

#include <cassert>
#include <unistd.h>
#include <pwd.h>
#include <algorithm>
#include <functional>  // for std::plus
#include <numeric>     // for std::accumulate
#include <iostream>
#include <fstream>
#include <vector>
#include "config.h"
#include "unicode/schriter.h"
#include "unicode/ucnv.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/Timer.h"
#include "ticcutils/FileUtils.h"
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
#define DBG *TiCC::Log(theDbgLog)

namespace Tokenizer {

  using namespace icu;
  using TiCC::operator<<;

  const UChar32 ZWJ = u'\u200D';

  const string ISO_SET = "http://raw.github.com/proycon/folia/master/setdefinitions/iso639_3.foliaset.ttl";

  const string UCTO_SET_PREFIX = "https://raw.githubusercontent.com/LanguageMachines/uctodata/master/setdefinitions/";

  const std::string Version() { return VERSION; }
  const std::string VersionName() { return PACKAGE_STRING; }

  TextCat *NEVERLAND = static_cast<TextCat*>((void*)0xABCD);

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

  bool TokenizerClass::setLangDetection( bool what ){
    /// set the doDetectLang option
    /*!
      \param what The new value.
      \return the old value

      When we set doDetectLang to true, we must assure that TextCat is
      initialized. So we attemp to initialize, which may fail if TextCat
      support is not available. In that case doDetectLang stays false
    */
    doDetectLang = what;
    if ( what ){
      doDetectLang = initialize_textcat();
    }
    return doDetectLang;
  }

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

  const UnicodeString type_separator = "SPACE";
  const UnicodeString type_currency = "CURRENCY";
  const UnicodeString type_emoticon = "EMOTICON";
  const UnicodeString type_picto = "PICTOGRAM";
  const UnicodeString type_word = "WORD";
  const UnicodeString type_symbol = "SYMBOL";
  const UnicodeString type_punctuation = "PUNCTUATION";
  const UnicodeString type_number = "NUMBER";
  const UnicodeString type_unknown = "UNKNOWN";
  const UnicodeString type_unanalyzed = "UNANALYZED";

  Token::Token( const UnicodeString& _type,
		const UnicodeString& _s,
		TokenRole _role, const string& _lang_code ):
    type(_type), us(_s), role(_role), lang_code(_lang_code) {
  }

  Token::Token( const UnicodeString& _type,
		const UnicodeString& _s,
		const string& _lang_code ):
    type(_type), us(_s), role(NOROLE), lang_code(_lang_code) {
  }


  std::string Token::texttostring() { return TiCC::UnicodeToUTF8(us); }
  std::string Token::typetostring() { return TiCC::UnicodeToUTF8(type); }

  ostream& operator<< (std::ostream& os, const Token& t ){
    os << t.type << " : " << t.role  << ": '" << t.us << "' (" << t.lang_code << ")";
    return os;
  }

  UnicodeString toUString( const TokenRole& tok ){
    UnicodeString result;
    if ( tok & NOSPACE){
      result += "NOSPACE ";
    }
    if ( tok & BEGINOFSENTENCE) {
      result += "BEGINOFSENTENCE ";
    }
    if ( tok & ENDOFSENTENCE) {
      result += "ENDOFSENTENCE ";
    }
    if ( tok & NEWPARAGRAPH) {
      result += "NEWPARAGRAPH ";
    }
    if ( tok & BEGINQUOTE) {
      result += "BEGINQUOTE ";
    }
    if ( tok & ENDQUOTE) {
      result += "ENDQUOTE ";
    }
    return result;
  }

  ostream& operator<<( ostream& os, const TokenRole& tok ){
    os << toUString( tok );
    return os;
  }

  bool TokenizerClass::initialize_textcat(){
#ifdef HAVE_TEXTCAT
    if ( text_cat ){
      return text_cat != NEVERLAND;
    }
    const char *homedir = getenv("HOME") ? getenv("HOME") : getpwuid(getuid())->pw_dir; //never NULL
    assert( homedir != NULL );
    const char *xdgconfighome = getenv("XDG_CONFIG_HOME"); //may be NULL
    const string localConfigDir = ((xdgconfighome != NULL) ? string(xdgconfighome) : string(homedir) + "/.config") + "/ucto/";
    //check local first
    string textcat_cfg = localConfigDir + "textcat.cfg";
    if (!TiCC::isFile(textcat_cfg)) {
        //check global second
        textcat_cfg = string(SYSCONF_PATH) + "/ucto/textcat.cfg";
        if (!TiCC::isFile(textcat_cfg)) {
	  LOG << "NO TEXTCAT SUPPORT DUE TO MISSING textcat.cfg!" << endl;
	  textcat_cfg = "";
        }
    }
    if (!textcat_cfg.empty()) {
      text_cat = new TextCat( textcat_cfg, theErrLog );
      LOG << " textcat configured from: " << textcat_cfg << endl;
    }
    else {
      text_cat = NEVERLAND; // signal invalidity
      return false;
    }
    //    text_cat->set_debug( true );
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
    return true;
#else
    LOG << "NO TEXTCAT SUPPORT!" << endl;
    return false;
#endif
  }

  TokenizerClass::TokenizerClass():
    linenum(0),
    inputEncoding( "UTF-8" ),
    space_separated(true),
    utt_mark("<utt>"),
    und_language(false),
    tokDebug(0),
    verbose(false),
    detectQuotes(false),
    doFilter(true),
    doPunctFilter(false),
    doWordCorrection(true),
    splitOnly( false ),
    detectPar(true),
    paragraphsignal(true),
    paragraphsignal_next(false),
    doDetectLang(false),
    text_redundancy("minimal"),
    sentenceperlineoutput(false),
    sentenceperlineinput(false),
    copyclass(false),
    lowercase(false),
    uppercase(false),
    xmlout(false),
    xmlin(false),
    passthru(false),
    ignore_tag_hints(false),
    ucto_processor(0),
    already_tokenized(false),
    inputclass("current"),
    outputclass("current"),
    text_cat( 0 )
  {
    theErrLog = new TiCC::LogStream(cerr, "ucto" );
    theDbgLog = theErrLog;
    theErrLog->setstamp( StampMessage );
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
    if ( theDbgLog != theErrLog ){
      delete theDbgLog;
    }
    delete theErrLog;
    if ( text_cat != NEVERLAND ){
      delete text_cat;
    }
  }

  bool TokenizerClass::reset( const string& lang ){
    ucto_processor = 0;
    already_tokenized = false;
    tokens.clear();
    if ( settings.find(lang) != settings.end() ){
      settings[lang]->quotes.clearStack();
    }
    return true;
  }

  bool TokenizerClass::setNormSet( const std::string& values ){
    /// set the normalization values
    /*!
      \param values The new stream
      \return true on succes

    */
    vector<string> parts = TiCC::split_at( values, "," );
    for ( const auto& val : parts ){
      norm_set.insert( TiCC::UnicodeFromUTF8( val ) );
    }
    return true;
  }

  void TokenizerClass::setErrorLog( TiCC::LogStream *os ) {
    /// set the theErrLog stream
    /*!
      \param os The new stream

      Cleans up the old ErrLog stream, except when still in use
    */
    if ( theErrLog != os
	 && theErrLog != theDbgLog ){
      delete theErrLog;
    }
    theErrLog = os;
  }

  void TokenizerClass::setDebugLog( TiCC::LogStream *os ) {
    /// set the theDbgLog stream
    /*!
      \param os The new stream

      Cleans up the old DbgLog stream, except when still in use
      May also set the debug_stream for TextCat, if applicable
    */
    if ( theDbgLog != os ){
      if ( text_cat != 0
	   && text_cat != NEVERLAND ) {
	text_cat->set_debug_stream( os );
      }
      if ( theDbgLog != theErrLog ){
	delete theDbgLog;
      }
    }
    theDbgLog = os;
  }

  string TokenizerClass::setInputEncoding( const std::string& enc ){
    /// set the encoding
    /*!
      \param enc The desired encoding. Should be supported bu ICU
      \return the old value
    */
    string old = inputEncoding;
    inputEncoding = enc;
    return old;
  }

  string TokenizerClass::setSeparators( const std::string& seps ){
    /// set the separators used to 'split' inputlines
    /*!
      \param seps A list of separator characters, will be added to
      the already known ones (if any)
      \return the previous value

      A '+' signals that space-like characters are to be used as separators
      A "-+" value of seps signals ONLY a '+' will separate
      Otherwise a \e seps line starting with '+' means that space-like
      characters, and all the following are separators

      e.g.:
      seps="+" means split on spaces (the default)
      seps="|[]" means only split on '|', '[' and ']'
      seps="+|" means split on spaces AND '|'
    */
    UnicodeString prev;
    if ( space_separated ){
      prev = "+";
    }
    prev = std::accumulate( separators.begin(), separators.end(),
			    prev, std::plus<UnicodeString>() );
    if ( !seps.empty() ){
      if ( seps == "+" ){
	// just use spacing characters as separators
	space_separated = true;
      }
      else if ( seps == "-+" ){
	// special case, to set ONLY a '+' as separator
	space_separated = false;
	separators.insert( seps[1] );
      }
      else {
	UnicodeString u_seps = TiCC::UnicodeFromUTF8( seps );
	for ( int i=0; i < u_seps.length(); ++i ) {
	  if ( u_seps[i] == '+' && i == 0 ){
	    // a '+' in the first position means use spacing separators
	    // AND all following.
	    space_separated = true;
	  }
	  else {
	    // add the character as a separator
	    separators.insert( u_seps[i] );
	  }
	}
      }
    }
    return TiCC::UnicodeToUTF8(prev);
  }

  string TokenizerClass::setTextRedundancy( const std::string& tr ){
    /// set the text_redundancy value
    /*!
      \param tr The desired text_redundancy
      \return the old value

      Valid values for \e tr are: "full", "minimal" and "none"
    */
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

  string TokenizerClass::setDocID( const string& id ) {
    /// set the docid value for FoLiA output
    /*!
      \param id The desired docid. Should be a valid NCName.
      \return the old value
    */
    const std::string s = docid;
    if ( folia::isNCName( id ) ){
      docid = id;
    }
    else {
      throw runtime_error( "can't set Document::id to: " + id
			   + ". Not a valid NCname" );
    }
    return s;
  }

  bool TokenizerClass::set_tc_debug( bool b ){
    /// set/unset the TexCat debug stream
    /*!
      \param b a boolean used to signal set/unset
      \return the old value

      Also makes sure TextCat is initialized
    */
    if ( !text_cat ){
      initialize_textcat();
    }
    if ( text_cat == NEVERLAND ){
      throw logic_error( "attempt to set debug on uninitialized TextClass object" );
    }
    else {
      return text_cat->set_debug( b );
    }
  }

  string fixup_UTF16( const string& input_line, const string& encoding ){
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

  folia::processor *TokenizerClass::init_provenance( folia::Document *doc,
						     folia::processor *parent ) const {
    if ( ucto_processor ){
      // already created
      if ( tokDebug > 0 ){
	DBG << "use already created processor: " << ucto_processor->id() << endl;
      }
      return ucto_processor;
    }
    if ( tokDebug > 0 ){
      cerr << "Init provenance" << endl;
    }
    vector<folia::processor *> procs = doc->get_processors_by_name( "ucto" );
    if ( !procs.empty() ){
      if ( procs.size() > 1 ){
	LOG << "ucto is very confused about '" << doc->filename() << "'\n"
	    << "Multiple 'ucto' processors have already been run?" << endl;
	exit( EXIT_FAILURE );
      }
      // ucto has been used one before, we can't do it complettely over again!
      LOG << "Difficult to tokenize '" << doc->filename()
	  << "' again, already processed by ucto before!" << endl;
      LOG << " The document will be copied as-is to the output file" << endl;
      already_tokenized = true;
      return procs[0];
    }
    else {
      folia::KWargs args;
      args["name"] = "ucto";
      args["generate_id"] = "auto()";
      args["version"] = PACKAGE_VERSION;
      args["command"] = _command;
      args["begindatetime"] = "now()";
      if ( parent ){
	ucto_processor = doc->add_processor( args, parent );
      }
      else {
	args["generator"] = "yes";
	ucto_processor = doc->add_processor( args );
	ucto_processor->get_system_defaults();
      }
      if ( tokDebug > 0 ){
	DBG << "created a new processor: " << ucto_processor->id() << endl;
      }
      return ucto_processor;
    }
  }

  folia::processor *TokenizerClass::add_provenance_passthru( folia::Document *doc,
							     folia::processor *parent ) const {
    folia::processor *proc = init_provenance( doc, parent );
    if ( proc ){
      folia::KWargs args;
      args["processor"] = proc->id();
      if ( tokDebug > 0 ){
	cerr << "declare( TOKEN, passthru, " << args << ")" << endl;
      }
      doc->declare( folia::AnnotationType::TOKEN, "passthru", args );
    }
    return proc;
  }

  folia::processor *TokenizerClass::add_provenance_undetermined( folia::Document *doc,
								 folia::processor *parent ) const {
    folia::processor *proc = init_provenance( doc, parent );
    if ( proc ){
      folia::KWargs args;
      args["processor"] = proc->id();
      if ( tokDebug > 0 ){
	cerr << "declare( TOKEN, undetermined, " << args << ")" << endl;
      }
      doc->declare( folia::AnnotationType::TOKEN, "undetermined", args );
    }
    return proc;
  }

  folia::processor *TokenizerClass::add_provenance_data( folia::Document *doc,
							 folia::processor* parent ) const {
    folia::processor *proc = init_provenance( doc, parent );
    if ( proc ){
      if ( !ucto_re_run() ){
	string id = "ucto.1.1";
	folia::processor *data_proc = doc->get_processor( id );
	if ( !data_proc ){
	  folia::KWargs args;
	  args["name"] = "uctodata";
	  args["generate_id"] = "auto()";
	  args["type"] = "datasource";
	  args["version"] = data_version;
	  data_proc = doc->add_processor( args, proc );
	}
	return data_proc;
      }
      else {
	return proc;
      }
    }
    else {
      return 0;
    }
  }

  folia::processor *TokenizerClass::add_provenance_structure(  folia::Document *doc,
							       const folia::AnnotationType type,
							       folia::processor *parent ) const {
    folia::processor *proc = init_provenance( doc, parent );
    if ( proc && !ucto_re_run() ){
      if ( !doc->declared( type ) ){
	// we can declare it
	folia::KWargs args;
	args["processor"] = proc->id();
	if ( tokDebug > 0 ){
	  cerr << "declare( " << type << ", None, " << args << ")" << endl;
	}
	doc->declare( type, "None", args );
	if ( tokDebug > 3 ){
	  DBG << "added " << folia::toString(type) << "-annotation for: '"
	      << proc->id() << endl;
	}
      }
      else {
	string proc_id = doc->default_processor(type);
	if ( !proc_id.empty() ){
	  proc = doc->get_processor(proc_id);
	  if ( tokDebug  ){
	    DBG << "REUSE  " << folia::toString(type) << "-annotation for: '"
		<< proc->id() << "' with set=" << doc->default_set(type) << endl;
	  }
	}
	else {
	  proc = 0;
	  if ( tokDebug  ){
	    DBG << "REUSE  " << folia::toString(type) << "-annotation"
		<< " with set=" << doc->default_set(type) << endl;
	  }
	}
      }
    }
    return proc;
  }

  folia::processor *TokenizerClass::add_provenance_structure( folia::Document *doc,
							      folia::processor *parent ) const {
    folia::processor *res = 0;
    add_provenance_structure( doc,
			      folia::AnnotationType::PARAGRAPH, parent );
    add_provenance_structure( doc,
			      folia::AnnotationType::SENTENCE,
			      parent );
    res = add_provenance_structure( doc,
				    folia::AnnotationType::QUOTE,
				    parent );
    return res;
  }

  folia::processor *TokenizerClass::add_provenance_setting( folia::Document *doc,
							    folia::processor *parent ) const {
    const folia::processor *proc = init_provenance( doc, parent );
    if ( proc && !ucto_re_run() ){
      folia::processor *data_proc = add_provenance_data( doc, parent );
      if ( !und_language
	   && doc->metadata_type() == "native" ){
	doc->set_metadata( "language", default_language );
      }
      for ( const auto& s : settings ){
	if ( tokDebug > 3 ){
	  DBG << "language: " << s.first << endl;
	}
	if ( s.first == "default" ){
	  continue;
	}
	if ( s.first == "und" ){
	  add_provenance_undetermined( doc, parent );
	  continue;
	}
	folia::KWargs args;
	args["name"] = s.second->set_file;
	args["generate_id"] = "next()";
	args["type"] = "datasource";
	args["version"] = s.second->version;
	doc->add_processor( args, data_proc );
	args.clear();
	args["processor"] = proc->id();
	string alias = config_prefix() + s.first;
	string ucto_set = UCTO_SET_PREFIX + alias + ".foliaset.ttl";
	args["alias"] = alias;
	if ( doc->declared( folia::AnnotationType::TOKEN, alias ) ){
	  // we assume that an old-style declaration is present
	  doc->un_declare( folia::AnnotationType::TOKEN, alias );
	}
	if ( tokDebug > 0 ){
	  cerr << "declare( TOKEN, " << ucto_set << ", " << args << ")" << endl;
	}
	doc->declare( folia::AnnotationType::TOKEN, ucto_set, args );
	if ( tokDebug > 3 ){
	  DBG << "added processor and token-annotation for: '"
	      << alias << "'" << endl;
	}
      }
      return data_proc;
    }
    else {
      return 0;
    }
  }

  folia::Document *TokenizerClass::start_document( const string& id ) const {
    folia::Document *doc = new folia::Document( "xml:id='" + id + "'" );
    doc->addStyle( "text/xsl", "folia.xsl" );
    if ( tokDebug > 3 ){
      DBG << "start document!!!" << endl;
    }
    if ( passthru ){
      add_provenance_passthru( doc );
    }
    else {
      if ( und_language ){
	add_provenance_undetermined( doc );
      }
      add_provenance_setting( doc );
    }
    folia::KWargs args;
    args["xml:id"] = doc->id() + ".text";
    doc->create_root<folia::Text>( args );
    return doc;
  }

#ifdef HAVE_TEXTCAT
  string TokenizerClass::detect( const UnicodeString& line ) {
    if ( text_cat == 0 ){
      initialize_textcat();
    }
    if ( text_cat == NEVERLAND ){
      return "";
    }
    UnicodeString temp = line;
    temp.findAndReplace( utt_mark, "" );
    temp.toLower();
    if ( tokDebug > 3 ){
      DBG << "use textCat to guess language from: "
	  << temp << endl;
    }
    string language = text_cat->get_language( TiCC::UnicodeToUTF8(temp) );
    string result;
    if ( settings.find( language ) != settings.end() ){
      if ( tokDebug > 4 ){
	DBG << "found a supported language: " << language << endl;
      }
      result = language;
    }
    else {
      if ( tokDebug > 3 ){
	DBG << "found an unsupported language: " << language << endl;
      }
      if ( und_language ){
	result = "und";
      }
      else {
	result = "default";
      }
    }
    return result;
  }
#else
  string TokenizerClass::detect( const UnicodeString& ) {
    LOG << "No TextCat support available" << endl;
    return "default";
  }
#endif

  vector<UnicodeString> TokenizerClass::sentence_split( const UnicodeString& in ){
    /// split a UnicodeString on the standard EOS markers,
    //  but ONLY when followed by a space. otherwise keep together
    set<int> eos_posses;
    UnicodeString EOSM = settings["default"]->eosmarkers;
    // first we collect al the positions of EOS markers
    for ( int i=0; i < EOSM.length(); ++i ){
      int pos = in.indexOf( EOSM[i] );
      while( pos >= 0 ){
	eos_posses.insert( pos );
	pos = in.indexOf( EOSM[i], pos+1 );
      }
    }
    // just in case ther is no EOS marker at the the end
    eos_posses.insert( in.length() -1 );
    // now we gather the results
    vector<UnicodeString> result;
    int prev = -1;
    bool last_had_space = true;
    for ( const auto& pos : eos_posses ){
      UnicodeString tmp = UnicodeString( in, prev+1, pos+1-prev );
      if ( last_had_space ){
	// new entry
	result.push_back( tmp );
      }
      else {
	// so no new entry but append
	result.back() += tmp;
      }
      last_had_space = u_isspace( tmp[tmp.length()-1] );
      prev = pos+1;
    }
    return result;
  }

  void TokenizerClass::tokenize_one_line( const UnicodeString& _input,
					  bool& bos,
					  const string& lang ){
    /// tokenize a UnicodeString into Token's
    /*!
      \param _input A UnicodeString
      \param bos An indicator that we know we are at a Begin Of Sentence
      \param lang the language to use for tokenizing

      As the Unicodestring can be of dubious heritage, we normalize
      it before further use
    */
    UnicodeString input_line = normalizer.normalize( _input );
    if ( passthru ){
      passthruLine( input_line, bos );
      return;
    }
    if ( und_language
	 && doDetectLang ){
      // hack into parts
      vector<UnicodeString> parts = sentence_split( input_line );
      if ( tokDebug > 3 ){
	cerr << "\nsplit RESULT: " << endl;
	for ( const auto& it : parts ){
	  cerr << "[" << it << "]" << endl;
	}
      }
      vector<pair<string,UnicodeString>> lang_parts;
      string cur_lang = "und";
      UnicodeString line;
      for ( const auto& part : parts ){
	string part_lang;
	vector<UnicodeString> tmp_v = TiCC::split( part );
	if ( tmp_v.size() > 3 ) {
	  part_lang = detect( part );
	}
	if ( part_lang.empty() ){
	  part_lang = cur_lang;
	}
	if ( part_lang != cur_lang ){
	  // language switch, so push old part
	  if ( !line.isEmpty() ){
	    lang_parts.push_back( make_pair(cur_lang, line) );
	    line.remove();
	  }
	  cur_lang = part_lang;
	}
	line += part;
	if ( part_lang == "und"
	     && ( ( part.lastIndexOf("?") == part.length()-2
		    || part.lastIndexOf("!") == part.length()-2 )
		  && part[part.length()-1] == ' ' ) ){
	  if ( !line.isEmpty() ){
	    lang_parts.push_back( make_pair(cur_lang, line) );
	    line.remove();
	  }
	}
      }
      if ( !line.isEmpty() ){
	lang_parts.push_back( make_pair(cur_lang,line) );
      }
      if ( tokDebug > 3 ){
	cerr << "\nlang_parts RESULT: " << endl;
	for ( const auto& it : lang_parts ){
	  cerr << "[" << it << "]" << endl;
	}
      }
      for ( const auto& part : lang_parts ){
	string lan = part.first;
	if ( lan == "und" ){
	  tokens.push_back( Token( type_unanalyzed, part.second, "und" ) );
	  tokens.back().role |= BEGINOFSENTENCE;
	  tokens.back().role |= ENDOFSENTENCE;
	}
	else {
	  internal_tokenize_line( part.second, part.first );
	}
      }
      return;
    }
    else {
      string language = lang;
      if ( language.empty() ){
	if ( tokDebug > 3 ){
	  DBG << "should we guess the language? "
	      << TiCC::toString(doDetectLang) << endl;
	}
	if ( doDetectLang ){
	  language = detect( input_line );
	  if ( tokDebug > 3 ){
	    DBG << "guessed language = '" << language << "'" << endl;
	  }
	}
      }
      internal_tokenize_line( input_line, language );
    }
  }

  vector<Token> TokenizerClass::tokenizeOneSentence( istream& IN ){
    if  (tokDebug > 0) {
      DBG << "[tokenizeOneSentence()] before countSent " << endl;
    }
    int numS = countSentences(); //count full sentences in token buffer
    if ( numS > 0 ) { // still some sentences in the buffer
      if  (tokDebug > 0) {
	DBG << "[tokenizeOneSentence] " << numS
	    << " sentence(s) in buffer, processing..." << endl;
      }
      return popSentence( );
    }
    if  (tokDebug > 0) {
      DBG << "[tokenizeOneSentence] NO sentences in buffer, searching.." << endl;
    }
    bool done = false;
    bool bos = true;
    inputEncoding = checkBOM( IN );
    string line;
    do {
      done = !getline( IN, line );
      UnicodeString input_line;
      if ( !done ){
	++linenum;
	if (tokDebug > 0) {
	  DBG << "[tokenize] Read input line " << linenum
	      << "-: '" << TiCC::format_non_printable( line ) << "'" << endl;
	}
	string tmp_line = fixup_UTF16( line, inputEncoding );
	if ( tokDebug > 0
	     && tmp_line != line ){
	  DBG << "After fixup, input_line= '"
	      << TiCC::format_non_printable( tmp_line ) << "'" << endl;
	}
	input_line = convert( tmp_line, inputEncoding );
	if ( sentenceperlineinput ){
	  input_line += " " + utt_mark;
	}
      }
      if  (tokDebug > 0) {
	DBG << "[tokenizeOneSentence] before next countSentences " << endl;
      }
      if ( done || input_line.isEmpty() ){
	//Signal the tokenizer that a paragraph is detected
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
	  DBG << "[tokenizeOneSentence] " << numS << " sentence(s) in buffer, processing first one..." << endl;
	}
	return popSentence();
      }
      else {
	if  (tokDebug > 0) {
	  DBG << "[tokenizeOneSentence] No sentence yet, reading on..." << endl;
	}
      }
    } while (!done);
    vector<Token> result;
    return result;
  }

  void TokenizerClass::appendText( folia::FoliaElement *root ) const {
    // set the textcontent of root to that of it's children
    if ( !root ){
      throw logic_error( "appendText() on empty root" );
    }
    if ( root->hastext( outputclass ) ){
      // there is already text at thus level, bail out.
      return;
    }
    if ( root->isSubClass( folia::Linebreak_t ) ){
      // exception
      return;
    }
    UnicodeString utxt;
    try {
      // so get Untokenized text from the children
      utxt = root->text( outputclass );
    }
    catch (...){
    }
    if ( !utxt.isEmpty() ){
      root->setutext( utxt, outputclass );
    }
    else if ( copyclass ){
      utxt = root->text( inputclass );
      if ( !utxt.isEmpty() ){
	root->setutext( utxt, outputclass );
      }
      else {
	throw logic_error( "still unable to set outputclass" );
      }
    }
    else {
      LOG << "unable to set text on node <" << root->xmltag() << " id='"
	  << root->id() << "'>. No text with outputclass= '" << outputclass
	  << "'" << endl;
      LOG << "maybe the inputfile is already tokenized, for inputclass='"
	  << inputclass << "' ?" << endl;
      LOG << "As a final resort you might try the --copyclass option." << endl;
      throw logic_error( "unable to set outputclass" );
    }
  }

  void removeText( folia::FoliaElement *root,
		   const string& outputclass  ){
    if ( !root ){
      throw logic_error( "removeText() on empty root" );
    }
    // remove the textcontent in outputclass of root
    root->clear_textcontent( outputclass );
  }

  folia::Document *TokenizerClass::tokenize( istream& IN ) {
    reset(); // when starting a new inputfile, we must reset provenance et.al.
    inputEncoding = checkBOM( IN );
    folia::Document *doc = start_document( docid );
    folia::FoliaElement *root = doc->doc()->index(0);
    int parCount = 0;
    vector<Token> buffer;
    do {
      if ( tokDebug > 0 ){
	DBG << "[tokenize] looping on stream" << endl;
      }
      vector<Token> v = tokenizeOneSentence( IN );
      if ( !v.empty() ){
	if ( tokDebug > 1 ){
	  DBG << "[tokenize] sentence=" << v << endl;
	}
	root = append_to_folia( root, v, parCount );
      }
    }
    while ( IN );
    if ( tokDebug > 0 ){
      DBG << "[tokenize] end of stream reached" << endl;
    }
    if (!buffer.empty()){
      if ( tokDebug > 1 ){
	DBG << "[tokenize] remainder=" << buffer << endl;
      }
      append_to_folia( root, buffer, parCount);
    }
    // make sure to set the text on the last root created
    if ( text_redundancy == "full" ){
      appendText( root );
    }
    else if ( text_redundancy == "none" ){
      removeText( root, outputclass );
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
      *OUT << *doc;
      OUT->flush();
      delete doc;
    }
    else {
      if ( ifile.empty() ){
	IN = &cin;
      }
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
      OUT << doc;
      OUT.flush();
      delete doc;
    }
#ifdef DO_READLINE
    else if ( &IN == &cin && isatty(0) ){
      // interactive use on a terminal (quite a hack..)
      const char *prompt = "ucto> ";
      int i = 0;
      while ( true ){
	string data;
	char *input = readline( prompt );
	if ( !input ){
	  break;
	}
	string line = input;
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
	  tokenizeLine( data );
	  // extract sentence from Token vector until done
	  vector<Token> v = popSentence();
	  while( !v.empty() ){
	    UnicodeString res = outputTokens( v , (i>0) );
	    OUT << res;
	    ++i;
	    v = popSentence();
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
	  DBG << "[tokenize] looping on stream" << endl;
	}
	vector<Token> v = tokenizeOneSentence( IN );
	while( !v.empty() ){
	  UnicodeString res = outputTokens( v , (i>0) );
	  OUT << res;
	  ++i;
	  v = tokenizeOneSentence( IN );
	}
      } while ( IN );
      if ( tokDebug > 0 ){
	DBG << "[tokenize] end_of_stream" << endl;
      }
      OUT << endl;
    }
  }

  void set_language( folia::FoliaElement* node, const string& lang ){
    // set the language on this @node to @lang
    // If a LangAnnotation with a set is already present, we silently
    // keep using that set.
    // Otherwise we add the ISO_SET
    string lang_set = node->doc()->default_set( folia::AnnotationType::LANG );
    if ( lang_set.empty() ){
      lang_set = ISO_SET;
      folia::KWargs args;
      args["processor"] = "ucto.1";
      node->doc()->declare( folia::AnnotationType::LANG,
			    ISO_SET,
			    args );
    }
    folia::KWargs args;
    args["class"] = lang;
    args["set"] = lang_set;
    folia::LangAnnotation *la = new folia::LangAnnotation( args, node->doc() );
    node->replace( la );
  }

  string get_parent_id( const folia::FoliaElement *el ){
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

  vector<folia::Word*> TokenizerClass::append_to_sentence( folia::Sentence *sent,
							   const vector<Token>& toks ) const {
    vector<folia::Word*> result;
    folia::Document *doc = sent->doc();
    string tok_set;
    if ( passthru ){
      tok_set = "passthru";
    }
    else {
      string tc_lc = get_language( toks );
      if ( tc_lc == "und" ){
	UnicodeString line;
	for ( const auto& tok : toks ){
	  line += tok.us;
	  if ( &tok != &toks.back() ){
	    line += " ";
	  }
	}
	set_language( sent, "und" );
	sent->setutext( line, outputclass );
	return result;
      }
      else if ( tc_lc != "default" ){
	tok_set = config_prefix() + tc_lc;
	set_language( sent, tc_lc );
      }
      else {
	tok_set = config_prefix() + default_language;
      }
    }
    folia::FoliaElement *root = sent;
    if ( tokDebug > 5 ){
      DBG << "add_words\n" << toks << endl;
    }
    for ( size_t i=0; i < toks.size(); ++i ){
      const auto& tok = toks[i];
      if ( tokDebug > 5 ){
	DBG << "add_result\n" << tok << endl;
      }
      if ( tok.role & BEGINQUOTE ){
	if  (tokDebug > 5 ) {
	  DBG << "[add_words] Creating quote element" << endl;
	}
	const folia::processor *proc
	  = add_provenance_structure( doc,
				      folia::AnnotationType::QUOTE );
	folia::KWargs args;
	string id = get_parent_id(root);
	if ( !id.empty() ){
	  args["generate_id"] = id;
	}
	if ( proc ){
	  args["processor"] = proc->id();
	}
	args["set"] = doc->default_set( folia::AnnotationType::QUOTE );
	folia::FoliaElement *quote = root->add_child<folia::Quote>( args );
	string quote_id = quote->id();
	// might need a new Sentence
	if ( i+1 < toks.size()
	     && toks[i+1].role & BEGINOFSENTENCE ){
	  const folia::processor *proc2
	    = add_provenance_structure( doc,
					folia::AnnotationType::SENTENCE );
	  folia::KWargs args2;
	  if ( !quote_id.empty() ){
	    args2["generate_id"] = quote_id;
	  }
	  if ( proc2 ){
	    args2["processor"] = proc2->id();
	  }
	  args2["set"] = doc->default_set( folia::AnnotationType::SENTENCE );
	  folia::Sentence *ns = quote->add_child<folia::Sentence>( args2 );
	  root = ns;
	}
	else {
	  root = quote;
	}
      }
      else if ( (tok.role & BEGINOFSENTENCE)
		&& root != sent
		&& root->element_id() == folia::Sentence_t ){
	// Ok, another Sentence in a quote
	if ( i > 0 && !(toks[i-1].role & BEGINQUOTE) ){
	  // close the current one, and start a new one.
	  // except when it is implicit created by a QUOTE
	  if ( tokDebug > 5 ){
	    DBG << "[add_words] next embedded sentence" << endl;
	  }
	  // honour text_redundancy on the Sentence
	  if ( text_redundancy == "full" ){
	    appendText( root );
	  }
	  else if ( text_redundancy == "none" ){
	    removeText( root, outputclass );
	  }
	  root = root->parent();
	  const folia::processor *proc
	    = add_provenance_structure( doc,
					folia::AnnotationType::SENTENCE );
	  folia::KWargs args;
	  string id = get_parent_id(root);
	  if ( !id.empty() ){
	    args["generate_id"] = id;
	  }
	  if ( proc ){
	    args["processor"] = proc->id();
	  }
	  args["set"] = doc->default_set( folia::AnnotationType::SENTENCE );
	  folia::Sentence *ns = root->add_child<folia::Sentence>( args );
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
      args["set"] = tok_set;
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
	  DBG << "create Word(" << args << ") = " << ws << endl;
	}
	folia::Word *w;
	try {
	  w = new folia::Word( args, doc );
	}
	catch ( const exception& e ){
	  cerr << "Word(" << args << ") creation failed: " << e.what() << endl;
	  exit(EXIT_FAILURE);
	}
	result.push_back( w );
	w->setutext( ws, outputclass );
	if ( tokDebug > 5 ){
	  DBG << "add_result, created a word: " << w << "(" << ws << ")" << endl;
	}
	root->append( w );
      }
      if ( tok.role & ENDQUOTE ){
	if ( i > 0
	     && toks[i-1].role & ENDOFSENTENCE ){
	  // end of quote implies with embedded Sentence
	  if ( tokDebug > 5 ){
	    DBG << "[add_words] End of quote" << endl;
	  }
	  // honour text_redundancy on the Sentence
	  if ( text_redundancy == "full" ){
	    appendText( root->parent() );
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
      appendText( sent );
    }
    else if ( text_redundancy == "none" ){
      removeText( sent, outputclass );
    }
    return result;
  }

  folia::FoliaElement *TokenizerClass::append_to_folia( folia::FoliaElement *root,
							const vector<Token>& tv,
							int& p_count ) const {
    if ( !root || !root->doc() ){
      throw logic_error( "missing root" );
    }
    if  ( tokDebug > 5 ){
      DBG << "append_to_folia, root = " << root << endl;
      DBG << "tokens=\n" << tv << endl;
    }
    if ( (tv[0].role & NEWPARAGRAPH) ) {
      if  ( tokDebug > 5 ){
	DBG << "append_to_folia, NEW paragraph " << endl;
      }
      const folia::processor *proc
	= add_provenance_structure( root->doc(),
				    folia::AnnotationType::PARAGRAPH );
      folia::KWargs args;
      if ( proc ){
	args["processor"] = proc->id();
      }
      args["set"] = root->doc()->default_set( folia::AnnotationType::PARAGRAPH );
      args["xml:id"] = root->doc()->id() + ".p." + TiCC::toString(++p_count);
      folia::Paragraph *p = new folia::Paragraph( args, root->doc() );
      if ( root->element_id() == folia::Text_t ){
	if  ( tokDebug > 5 ){
	  DBG << "append_to_folia, add paragraph to Text" << endl;
	}
	root->append( p );
      }
      else {
	// root is a paragraph, which is done now.
	if ( text_redundancy == "full" ){
	  root->setutext( root->unicode(outputclass), outputclass);
	}
	if  ( tokDebug > 5 ){
	  DBG << "append_to_folia, add paragraph to parent of " << root << endl;
	}
	root = root->parent();
	root->append( p );
      }
      root = p;
    }
    const folia::processor *proc
      = add_provenance_structure( root->doc(),
				  folia::AnnotationType::SENTENCE );
    folia::KWargs args;
    if ( proc ){
      args["processor"] = proc->id();
    }
    args["set"] = root->doc()->default_set( folia::AnnotationType::SENTENCE );
    args["generate_id"] = root->id();
    folia::Sentence *s = root->add_child<folia::Sentence>( args );
    if  ( tokDebug > 5 ){
      DBG << "append_to_folia, created Sentence" << s << endl;
    }
    append_to_sentence( s, tv );
    return root;
  }

  UnicodeString handle_token_tag( const folia::FoliaElement *d,
				  const folia::TextPolicy& tp ){
    /// a handler that is passed on to libfolia to handle special tag="token"
    /// nodes
    /*!
      \param d The FoliaElement that libfolia will handle us
      \param tp The TextPolicy at hand. This function has been registered in
      \em tp
      \return a UnicodeString which we will mark specially so that we know
      that this string is to be handled as a separate token

      This function will be called by libfolia's text() functions on
      encountering a tag="token" attribute in a TextContent.
      It has to be registered in \em tp
     */
    UnicodeString tmp_result = text( d, tp );
    tmp_result = ZWJ + tmp_result;
    tmp_result += ZWJ;
    return tmp_result;
  }

  void TokenizerClass::correct_element( folia::FoliaElement *orig,
					const vector<Token>& toks,
					const string& tok_set ) const {
    vector<folia::FoliaElement*> sV;
    vector<folia::FoliaElement*> cV;
    vector<folia::FoliaElement*> oV;
    vector<folia::FoliaElement*> nV;
    // Original element
    oV.push_back( orig );
    // Add the edits
    for ( const auto& tok : toks ){
      // New elements
      folia::KWargs args;
      args["xml:id"] = orig->generateId( "tokenized" );
      args["class"] = TiCC::UnicodeToUTF8(tok.type);
      if ( tok.role & NOSPACE ){
	args["space"] = "no";
      }
      if ( outputclass != "current" ){
	args["textclass"] = outputclass;
      }
      args["set"] = tok_set;
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
	  DBG << "create Word(" << args << ") = " << ws << endl;
	}
	folia::FoliaElement *new_elt;
	try {
	  new_elt = folia::AbstractElement::createElement( orig->element_id(),
							   orig->doc() );
	  new_elt->setAttributes( args );
	}
	catch ( const exception& e ){
	  cerr << orig->tag() << "(" << args << ") creation failed: "
	       << e.what() << endl;
	  exit(EXIT_FAILURE);
	}
	new_elt->setutext( ws, outputclass );
	if ( tokDebug > 5 ){
	  DBG << "add_result, created: " << new_elt << "(" << ws << ")" << endl;
	}
	nV.push_back( new_elt );
      }
    }
    folia::KWargs no_args;
    no_args["processor"] = ucto_processor->id();
    no_args["set"] = tok_set;
    folia::Correction *c = orig->parent()->correct( oV, cV, nV, sV, no_args );
    if ( tokDebug > 2 ){
      DBG << "created: " << c->xmlstring() << endl;
    }
    else if ( tokDebug > 0 ){
      DBG << "created: " << c << endl;
    }
  }

  vector<Token> TokenizerClass::correct_elements( folia::FoliaElement *e,
						  const vector<folia::FoliaElement*>& wv ) {
    // correct_elements may be called directly from Frog, so make sure to set TP
    text_policy.set_class( inputclass );
    vector<Token> result;
    // correct only when the sentence is in the desired language
    string s_la;
    if ( e->has_annotation<folia::LangAnnotation>() ){
      s_la = e->annotation<folia::LangAnnotation>()->cls();
    }
    if ( !s_la.empty() && settings.find(s_la) == settings.end() ){
      // the Sentence already has a language code, and it
      // is NOT what we search for.
      // just ignore it
      if ( tokDebug > 0 ){
	DBG << "skip FoLiA element " << e->id() << " with unsupported language "
	    << s_la << endl;
      }
      return result;
    }
    string tok_set;
    if ( !s_la.empty() ){
      tok_set = config_prefix() + s_la;
    }
    else {
      tok_set = config_prefix() + default_language;
    }
    folia::KWargs args;
    args["processor"] = ucto_processor->id();
    e->doc()->declare( folia::AnnotationType::CORRECTION, tok_set, args );
    for ( auto w : wv ){
      UnicodeString text = w->unicode( text_policy );
      if ( tokDebug > 0 ){
	DBG << "correct_elements() text='" << text << "'" << endl;
      }
      tokenizeLine( text );
      vector<Token> sent = popSentence();
      while ( sent.size() > 0 ){
	sent.front().role &= ~BEGINOFSENTENCE;
	sent.back().role &= ~ENDOFSENTENCE;
	result.insert( result.end(), sent.begin(), sent.end() );
	correct_element( w, sent, tok_set );
	sent = popSentence();
      }
    }
    result.front().role |= BEGINOFSENTENCE;
    result.back().role |= ENDOFSENTENCE;
    return result;
  }

  void TokenizerClass::handle_one_sentence( folia::Sentence *s,
					    int& sentence_done ){
    // check feasibility
    if ( tokDebug > 1 ){
      DBG << "handle_one_sentence: " << s << endl;
    }
    if ( inputclass != outputclass && outputclass == "current" ){
      if ( s->hastext( outputclass ) ){
	throw uLogicError( "cannot set text with class='current' on node "
			   + s->id() +
			   " because it already has text in that class." );
      }
    }
    vector<folia::Word *> wv = s->words( inputclass );
    if ( wv.empty() ){
      wv = s->words();
    }
    if ( !wv.empty() ){
      // there are already words.
      if ( doWordCorrection ){
	// we are allowed to correct those
	vector<folia::FoliaElement*> ev(wv.begin(),wv.end());
	if ( !correct_elements( s, ev ).empty() ){
	  ++sentence_done;
	}
      }
    }
    else {
      string s_la;
      if ( s->has_annotation<folia::LangAnnotation>() ){
	s_la = s->annotation<folia::LangAnnotation>()->cls();
      }
      if ( !s_la.empty()
	   && settings.find(s_la) == settings.end() ){
	// the Sentence already has a language code, and it
	// is NOT what we search for.
	// just ignore it
	if ( tokDebug > 0 ){
	  DBG << "skip sentence " << s->id() << " with unsupported language "
	      << s_la << endl;
	}
	return;
      }
      UnicodeString text = s->unicode( text_policy );
      if ( tokDebug > 0 ){
	DBG << "handle_one_sentence() from string: '" << text << "'" << endl;
      }
      tokenizeLine( text );
      vector<Token> sent = popSentence();
      while ( sent.size() > 0 ){
	append_to_sentence( s, sent );
	if  (tokDebug > 0){
	  DBG << "created a new sentence: " << s << endl;
	}
	++sentence_done;
	sent = popSentence();
      }
    }
    if ( text_redundancy == "full" ){
      appendText( s );
    }
    else if ( text_redundancy == "none" ){
      removeText( s, outputclass );
    }
  }

  void TokenizerClass::handle_one_paragraph( folia::Paragraph *p,
					     int& sentence_done ){
    // a Paragraph may contain both Word and Sentence nodes
    // Sentences will be handled
    vector<folia::Sentence*> sv = p->select<folia::Sentence>(false);
    if ( sv.empty() ){
      // No Sentence, so just text or Words
      vector<folia::Word*> wv = p->select<folia::Word>(false);
      if ( !wv.empty() ){
	vector<folia::FoliaElement*> ev( wv.begin(), wv.end() );
	// Words found
	if ( doWordCorrection ){
	  if ( correct_elements( p, ev ).empty() ){
	    ++sentence_done;
	  }
	}
	// otherwise skip
      }
      else {
	// No Words too, handle text, if any
	UnicodeString text = p->unicode( text_policy );
	if ( tokDebug > 0 ){
	  DBG << "handle_one_paragraph:" << text << endl;
	}
	tokenizeLine( text );
	vector<Token> toks = popSentence();
	const folia::processor *proc = 0;
	while ( !toks.empty() ){
	  if ( proc == 0 ){
	    proc = add_provenance_structure( p->doc(),
					     folia::AnnotationType::SENTENCE );
	  }
	  string p_id = p->id();
	  folia::KWargs args;
	  if ( proc ){
	    args["processor"] = proc->id();
	  }
	  args["set"] = p->doc()->default_set(folia::AnnotationType::SENTENCE);
	  if ( !p_id.empty() ){
	    args["generate_id"] = p_id;
	  }
	  folia::Sentence *s = p->add_child<folia::Sentence>( args );
	  append_to_sentence( s, toks );
	  if  (tokDebug > 0){
	    DBG << "created a new sentence: " << s << endl;
	  }
	  ++sentence_done;
	  toks = popSentence();
	}
      }
    }
    else {
      if ( tokDebug > 1 ){
	DBG << "found some Sentences " << sv << endl;
      }
      // For now wu just IGNORE loose words (backward compatability)
      for ( const auto& s : sv ){
	handle_one_sentence( s, sentence_done );
      }
    }
    if ( text_redundancy == "full" ){
      appendText( p );
    }
    else if ( text_redundancy == "none" ){
      removeText( p, outputclass );
    }
  }

  void TokenizerClass::handle_one_text_parent( folia::FoliaElement *e,
					       int& sentence_done ){
    ///
    /// input is a FoLiA element @e containing text, direct or deeper
    /// this can be a Word, Sentence, Paragraph or some other element
    /// In the latter case, we construct a Sentence from the text, and
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
	DBG << "found text in a sentence " << e << endl;
      }
      handle_one_sentence( dynamic_cast<folia::Sentence*>(e),
			   ++sentence_done );
    }
    else if ( e->xmltag() == "p" ){
      // OK a longer text in some paragraph, maybe more sentences
      if ( tokDebug > 2 ){
	DBG << "found text in a paragraph " << e << endl;
      }
      handle_one_paragraph( dynamic_cast<folia::Paragraph*>(e),
			    sentence_done );
    }
    else {
      // Some text outside word, paragraphs or sentences (yet)
      // mabe <div> or <note> or such
      // there may be embedded Paragraph, Word and Sentence nodes
      // if so, Paragraphs and Sentences should be handled separately
      vector<folia::Sentence*> sv = e->select<folia::Sentence>(false);
      vector<folia::Paragraph*> pv = e->select<folia::Paragraph>(false);
      if ( pv.empty() && sv.empty() ){
	// just words or text
	UnicodeString text = e->unicode( text_policy );
	if ( tokDebug > 1 ){
	  DBG << "tok-" << e->xmltag() << ":" << text << endl;
	}
	tokenizeLine( text );
	vector<vector<Token>> sents;
	vector<Token> toks = popSentence();
	while ( toks.size() > 0 ){
	  sents.push_back( toks );
	  toks = popSentence();
	}
	if ( sents.size() == 0 ){
	  // can happen in very rare cases (strange spaces in the input)
	  // SKIP!
	}
	else if ( sents.size() > 1 ){
	  // multiple sentences. We need an extra Paragraph.
	  // But first check if this is allowed!
	  folia::FoliaElement *rt;
	  if ( e->acceptable(folia::Paragraph_t) ){
	    folia::KWargs args;
	    string e_id = e->id();
	    if ( !e_id.empty() ){
	      args["generate_id"] = e_id;
	    }
	    const folia::processor *proc
	      = add_provenance_structure( e->doc(),
					  folia::AnnotationType::PARAGRAPH );
	    if ( proc ){
	      args["processor"] = proc->id();
	    }
	    args["set"] = e->doc()->default_set( folia::AnnotationType::PARAGRAPH );
	    folia::Paragraph *p = e->add_child<folia::Paragraph>( args );
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
	    const folia::processor *proc
	      = add_provenance_structure( e->doc(),
					  folia::AnnotationType::SENTENCE );
	    if ( proc ){
	      args["processor"] =  proc->id();
	    }
	    args["set"] = e->doc()->default_set( folia::AnnotationType::SENTENCE );
	    folia::Sentence *s = rt->add_child<folia::Sentence>( args );
	    append_to_sentence( s, sent );
	    ++sentence_done;
	    if  (tokDebug > 0){
	      DBG << "created a new sentence: " << s << endl;
	    }
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
	  const folia::processor *proc
	    = add_provenance_structure( e->doc(),
					folia::AnnotationType::SENTENCE );
	  if ( proc ){
	    args["processor"] =  proc->id();
	  }
	  args["set"] = e->doc()->default_set( folia::AnnotationType::SENTENCE );
	  folia::Sentence *s = e->add_child<folia::Sentence>( args );
	  append_to_sentence( s, sents[0] );
	  ++sentence_done;
	  if  (tokDebug > 0){
	    DBG << "created a new sentence: " << s << endl;
	  }
	}
      }
      else if ( !pv.empty() ){
	if ( tokDebug > 1 ){
	  DBG << "found some Paragraphs " << pv << endl;
	}
	// For now we only handle the Paragraphs, ignore sentences and words
	// IS this even valid???
	for ( const auto& p : pv ){
	  handle_one_paragraph( p, sentence_done );
	}
      }
      else {
	if ( tokDebug > 1 ){
	  DBG << "found some Sentences " << sv << endl;
	}
	// For now we just IGNORE the loose words (backward compatability)
	for ( const auto& s : sv ){
	  handle_one_sentence( s, sentence_done );
	}
      }
    }
    if ( text_redundancy == "full" ){
      appendText( e );
    }
    else if ( text_redundancy == "none" ){
      removeText( e, outputclass );
    }
  }

  folia::Document *TokenizerClass::tokenize_folia( const string& infile_name ){
    reset(); // when starting a new inputfile, we must reset provenance et.al.
    if ( inputclass == outputclass
	 && !doWordCorrection ){
      DBG << "ucto: --filter=NO is automatically set. inputclass equals outputclass!"
	  << endl;
      setFiltering(false);
    }
    text_policy.set_class( inputclass );
    if ( !ignore_tag_hints ){
      text_policy.add_handler("token", &handle_token_tag );
    }
    folia::TextEngine proc( infile_name );
    if ( passthru ){
      add_provenance_passthru( proc.doc() );
    }
    else {
      add_provenance_setting( proc.doc() );
    }
    if  ( tokDebug > 8){
      proc.set_dbg_stream( theErrLog );
      proc.set_debug( true );
    }
    //    proc.set_debug( true );
    proc.setup( inputclass, true );
    int sentence_done = 0;
    folia::FoliaElement *p = 0;
    folia::FoliaElement *parent = 0;
    while ( (p = proc.next_text_parent() ) ){
      //      DBG << "next text parent: " << p << endl;
      if ( !parent ){
	parent = p->parent();
	//	DBG << "my parent: " << parent << endl;
      }
      if ( already_tokenized ){
	++sentence_done;
      }
      else {
	handle_one_text_parent( p, sentence_done );
      }
      if ( tokDebug > 0 ){
	DBG << "done with sentence " << sentence_done << endl;
      }
      if ( proc.next() ){
	if ( tokDebug > 1 ){
	  DBG << "looping for more ..." << endl;
	}
      }
    }
    if ( text_redundancy == "full" ){
      appendText( parent );
    }
    else if ( text_redundancy == "none" ){
      removeText( parent, outputclass );
    }
    if ( sentence_done == 0 ){
      LOG << "document contains no text in the desired inputclass: "
	  << inputclass << endl;
      LOG << "NO result!" << endl;
      return 0;
    }
    return proc.doc(true); // take the doc over from the Engine
  }

  void TokenizerClass::tokenize_folia( const string& infile_name,
				       const string& outfile_name ){
    if ( tokDebug > 0 ){
      DBG << "[tokenize_folia] (" << infile_name << ","
	  << outfile_name << ")" << endl;
    }
    const folia::Document *doc = tokenize_folia( infile_name );
    if ( doc ){
      doc->save( outfile_name, false );
      if ( tokDebug > 0 ){
	DBG << "resulting FoLiA doc saved in " << outfile_name << endl;
      }
    }
    else {
      if ( tokDebug > 0 ){
	DBG << "NO FoLiA doc created! " << endl;
      }
    }
  }

  UnicodeString TokenizerClass::outputTokens( const vector<Token>& tokens,
					      const bool continued ) const {
    /*!
      \param tokens A list of Token's to display
      \param continued Set to true when outputTokens is invoked multiple
      times and it is not the first invokation

      this makes paragraph boundaries work over multiple calls
      \return A UnicodeString representing tokenized lines, including token
      information, when verbose mode is on.
    */
    short quotelevel = 0;
    UnicodeString result;
    for ( const auto& token : tokens ) {
      UnicodeString outline;
      if (tokDebug >= 5){
	DBG << "outputTokens: token=" << token << endl;
      }
      if ( detectPar
	   && (token.role & NEWPARAGRAPH)
	   && !verbose
	   && continued ) {
	//output paragraph separator
	if ( sentenceperlineoutput ) {
	  outline += "\n";
	}
	else {
	  outline += "\n\n";
	}
      }
      UnicodeString s = token.us;
      if (lowercase) {
	s = s.toLower();
      }
      else if ( uppercase ) {
	s = s.toUpper();
      }
      outline += s;
      if ( token.role & NEWPARAGRAPH ) {
	quotelevel = 0;
      }
      if ( token.role & BEGINQUOTE ) {
	++quotelevel;
      }
      if ( verbose ) {
	outline += "\t" + token.type + "\t" + toUString(token.role) + "\n";
      }
      if ( token.role & ENDQUOTE ) {
	--quotelevel;
      }

      if ( token.role & ENDOFSENTENCE ) {
	if ( verbose ) {
	  if ( !(token.role & NOSPACE ) ){
	    outline += "\n";
	  }
	}
	else {
	  if ( quotelevel == 0 ) {
	    if ( sentenceperlineoutput ) {
	      outline += "\n";
	    }
	    else {
	      outline += " " + utt_mark + " ";
	    }
	    if ( splitOnly ){
	      outline += "\n";
	    }
	  }
	  else { //inside quotation
	    if ( splitOnly
		 && !(token.role & NOSPACE ) ){
	      outline += " ";
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
	      outline += " ";
	    }
	  }
	}
	else if ( quotelevel > 0 ) {
	  //FBK: ADD SPACE WITHIN QUOTE CONTEXT IN ANY CASE
	  outline += " ";
	}
      }
      if (tokDebug >= 5){
	DBG << "outputTokens: outline=" << outline << endl;
      }
      result += outline;
    }
    if (tokDebug >= 5){
      DBG << "outputTokens: result= '" << result << "'" << endl;
    }
    return result;
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
    string cur_lang;
    int tok_cnt = 0;
    for ( auto& token : tokens ) {
      if ( cur_lang.empty() ){
	cur_lang = token.lang_code;
      }
      else if ( token.lang_code != cur_lang ){
	tokens[tok_cnt-1].role |= ENDOFSENTENCE;
	cur_lang = token.lang_code;
      }
      if (tokDebug >= 5){
	DBG << "[countSentences] buffer#" << tok_cnt
	    << " token=[ " << token << " ], quotelevel="<< quotelevel << endl;
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
	begin = tok_cnt + 1;
	count++;
	if (tokDebug >= 5){
	  DBG << "[countSentences] SENTENCE #" << count << " found" << endl;
	}
      }
      if ( forceentirebuffer
	   && ( tok_cnt == size - 1)
	   && !(token.role & ENDOFSENTENCE) )  {
	//last token of buffer
	count++;
	token.role |= ENDOFSENTENCE;
	if (tokDebug >= 5){
	  DBG << "[countSentences] SENTENCE #" << count << " *FORCIBLY* ended" << endl;
	}
      }
      ++tok_cnt;
    }
    if (tokDebug >= 5){
      DBG << "[countSentences] end of loop: returns " << count << endl;
    }
    return count;
  }

  vector<Token> TokenizerClass::popSentence( ) {
    vector<Token> outToks;
    const int size = tokens.size();
    if ( size != 0 ){
      short quotelevel = 0;
      size_t begin = 0;
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
	  size_t end = i;
	  if (tokDebug >= 1){
	    DBG << "[tokenize] extracted sentence, begin=" << begin
		<< ",end="<< end << endl;
	  }
	  for ( size_t index=begin; index <= end; ++index ){
	    outToks.push_back( tokens[index] );
	  }
	  tokens.erase( tokens.begin(), tokens.begin()+end+1 );
	  if ( !passthru ){
	    string lang = get_language( outToks );
	    if ( lang != "und" ){
	      if ( !settings[lang]->quotes.emptyStack() ) {
		settings[lang]->quotes.flushStack( end+1 );
	      }
	    }
	  }
	  // we are done...
	  return outToks;
	}
      }
    }
    return outToks;
  }

  UnicodeString TokenizerClass::getString( const vector<Token>& v ){
    if ( !v.empty() ){
      //This only makes sense in non-verbose mode, force verbose=false
      const bool tv = verbose;
      verbose = false;
      UnicodeString res = outputTokens( v );
      verbose = tv;
      return res;
    }
    return "";
  }

  string TokenizerClass::getUTF8String( const vector<Token>& v ){
    UnicodeString result = getString( v );
    return TiCC::UnicodeToUTF8( result );
  }

  vector<UnicodeString> TokenizerClass::getSentences() {
    vector<UnicodeString> sentences;
    if  (tokDebug > 0) {
      DBG << "[getSentences()] before countSent " << endl;
    }
    int numS = countSentences(true); // force buffer to end with END_OF_SENTENCE
    if  (tokDebug > 0) {
      DBG << "[getSentences] found " << numS << " sentence(s)" << endl;
    }
    for (int i = 0; i < numS; i++) {
      vector<Token> v = popSentence( );
      UnicodeString tmp = getString( v );
      sentences.push_back( tmp );
    }
    return sentences;
  }

  vector<string> TokenizerClass::getUTF8Sentences() {
    vector<UnicodeString> uv = getSentences();
    vector<string> result;
    std::transform( uv.begin(), uv.end(),
		    result.begin(),
		    []( const UnicodeString& us ){ return TiCC::UnicodeToUTF8(us); });
    return result;
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
	DBG << "[resolveQuote] Quote found, begin="<< beginindex << ", end="<< endindex << endl;
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
	  DBG << "marked BEGIN: " << tokens[beginindex] << endl;
	  DBG << "marked   END: " << tokens[endindex] << endl;
	}
      }
      else if ( expectingend == 1
		&& subquote == 0
		&& !( tokens[endindex - 1].role & ENDOFSENTENCE) ) {
	//missing one endofsentence, we can correct, last token in quote token is endofsentence:
	if ( tokDebug >= 2 ) {
	  DBG << "[resolveQuote] Missing endofsentence in quote, fixing... " << expectingend << endl;
	}
	tokens[endindex - 1].role |= ENDOFSENTENCE;
	//mark the quote
	tokens[beginindex].role |= BEGINQUOTE;
	tokens[endindex].role |= ENDQUOTE;
      }
      else {
	if ( tokDebug >= 2) {
	  DBG << "[resolveQuote] Quote can not be resolved, unbalanced sentences or subquotes within quote, skipping... (expectingend=" << expectingend << ",subquote=" << subquote << ")" << endl;
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
	c = tokens[i+1].us.char32At(0);
	if ( u_isquote( c, quotes ) ){
	  // next word is quote
	  if ( detectQuotes ){
	    is_eos = true;
	  }
	  else if ( i + 2 < tokens.size() ) {
	    c = tokens[i+2].us.char32At(0);
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
	DBG << "[detectQuoteBounds] Standard double-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c,quotes)) {
	if (tokDebug > 1 ) {
	  DBG << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else if ( c == '\'' ) {
      if (tokDebug > 1 ){
	DBG << "[detectQuoteBounds] Standard single-quote (ambiguous) found @i="<< i << endl;
      }
      if (!resolveQuote(i,c,quotes)) {
	if (tokDebug > 1 ) {
	  DBG << "[detectQuoteBounds] Doesn't resolve, so assuming beginquote, pushing to stack for resolution later" << endl;
	}
	quotes.push( i, c );
      }
    }
    else {
      UnicodeString close = quotes.lookupOpen( c );
      if ( !close.isEmpty() ){ // we have a opening quote
	if ( tokDebug > 1 ) {
	  DBG << "[detectQuoteBounds] Opening quote found @i="<< i << ", pushing to stack for resolution later..." << endl;
	}
	quotes.push( i, c ); // remember it
      }
      else {
	UnicodeString open = quotes.lookupClose( c );
	if ( !open.isEmpty() ) { // we have a closing quote
	  if (tokDebug > 1 ) {
	    DBG << "[detectQuoteBounds] Closing quote found @i="<< i << ", attempting to resolve..." << endl;
	  }
	  if ( !resolveQuote( i, open, quotes )) {
	    // resolve the matching opening
	    if (tokDebug > 1 ) {
	      DBG << "[detectQuoteBounds] Unable to resolve" << endl;
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
	DBG << method << " i="<< i << " token=[ " << tokens[i] << " ]" << endl;
      }
      if ( tokens[i].type.startsWith("PUNCTUATION") ){
	if ((tokDebug > 1 )){
	  DBG << method << " PUNCTUATION FOUND @i=" << i << endl;
	}
	// we have some kind of punctuation. Does it mark an eos?
	bool is_eos = detectEos( i,
				 settings[lang]->eosmarkers,
				 settings[lang]->quotes );
	is_eos &= !sentenceperlineinput;
	if ( is_eos) {
	  // end of sentence found/ so wrap up
	  if ( detectQuotes
	       && !settings[lang]->quotes.emptyStack() ) {
	    // we have some quotes!
	    if ( tokDebug > 1 ){
	      DBG << method << " Unbalances quotes: Preliminary EOS FOUND @i="
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
	      DBG << method << " EOS FOUND @i=" << i << endl;
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
	    DBG << method << " Close FOUND @i=" << i << endl;
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
    for ( int i = size-1; i > offset; --i ) {
      // at the end of the buffer there may be some PUNCTUATION which
      // has spurious ENDOFSENTENCE and BEGINOFSENTENCE annotation
      // fix this up to avoid sentences containing only punctuation
      // also we don't want a BEGINQUOTE to be an ENDOFSENTENCE
      if ( tokDebug > 2 ){
	DBG << method << " fixup-end i="<< i << " token=[ " << tokens[i]
	    << " ]" << endl;
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
      else {
	break;
      }
    }
    if (tokDebug > 6 ){
      DBG << "After Fixup" << endl;
      for (int i = offset; i < size; i++) {
	DBG << method << " i="<< i << " token=[ " << tokens[i] << " ]" << endl;
      }
    }
  }

  bool TokenizerClass::is_separator( UChar32 c ){
    bool result = false;
    if ( space_separated ){
      result = u_isspace( c );
    }
    result |= ( separators.find( c ) != separators.end() );
    return result;
  }

  void TokenizerClass::passthruLine( const UnicodeString& input, bool& bos ) {
    if (tokDebug) {
      DBG << "[passthruLine] input: line=[" << input << "]" << endl;
    }
    bool alpha = false, num = false, punct = false;
    UnicodeString word;
    StringCharacterIterator sit(input);
    while ( sit.hasNext() ){
      UChar32 c = sit.current32();
      if ( c == ZWJ ){
	// a joiner. just ignore
	sit.next32();
	continue;
      }
      if ( is_separator(c) ) {
	if ( word.isEmpty() ){
	  // a leading space. Don't waste time on it. SKIP
	  sit.next32();
	  continue;
	}
	// so a trailing space. handle the found word.
	if (tokDebug){
	  DBG << "[passthruLine] word=[" << word << "]" << endl;
	}
	if ( word == utt_mark ) {
	  word = "";
	  if ( !tokens.empty() ){
	    tokens.back().role |= ENDOFSENTENCE;
	  }
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
	      DBG << "   [passThruLine] skipped PUNCTUATION ["
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
	    if ( bos ) {
	      tokens.push_back( Token( type, word , BEGINOFSENTENCE, "default" ) );
	      bos = false;
	    }
	    else {
	      tokens.push_back( Token( type, word, "default" ) );
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
      if ( word == utt_mark ) {
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
	    DBG << "   [passThruLine] skipped PUNCTUATION ["
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
	  if ( bos ) {
	    tokens.push_back( Token( type, word , BEGINOFSENTENCE, "default" ) );
	    bos = false;
	  }
	  else {
	    tokens.push_back( Token( type, word, "default" ) );
	  }
	}
      }
    }
    if ( sentenceperlineinput && tokens.size() > 0 ) {
      tokens[0].role |= BEGINOFSENTENCE;
      tokens.back().role |= ENDOFSENTENCE;
    }
    if ( tokDebug > 5 ){
      DBG << "[passthruLine] END result:" << endl;
      for ( const auto& tok : tokens ){
	DBG << "\t[passthruLine] token=[" << tok << "]" << endl;
      }
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
    const char *encoding = ucnv_detectUnicodeSignature( s.c_str(),
							s.length(),
							&bomLength,
							&err);
    if ( bomLength ){
      if ( tokDebug ){
	DBG << "Autodetected encoding: " << encoding << endl;
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
  void TokenizerClass::tokenizeLine( const string& s,
				     const string& lang ){
    UnicodeString us = convert( s, inputEncoding );
    tokenizeLine( us, lang );
  }

  // UnicodeString wrapper
  void TokenizerClass::tokenizeLine( const UnicodeString& us,
				     const string& lang ){
    bool bos = true;
    tokenize_one_line( us, bos, lang );
    if  (tokDebug > 0) {
      DBG << "[tokenizeLine()] before countSent " << endl;
    }
    countSentences(true); // force the ENDOFSENTENCE
  }

  bool u_isemo( UChar32 c ){
    UBlockCode s = ublock_getCode(c);
    return s == UBLOCK_EMOTICONS;
  }

  bool u_isothernumber( UChar32 c ){
    return u_charType( c ) == U_OTHER_NUMBER;
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

  const UnicodeString& TokenizerClass::detect_type( UChar32 c ){
    if ( is_separator(c)) {
      return type_separator;
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
    else if ( u_isothernumber(c)) {
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

  int TokenizerClass::internal_tokenize_line( const UnicodeString& originput,
					      const string& _lang ){
    if ( originput.isBogus() ){ //only tokenize valid input
      LOG << "ERROR: Invalid UTF-8 in line:" << linenum << endl
	  << "   '" << originput << "'" << endl;
      return 0;
    }
    string lang = _lang;
    if ( lang.empty() ){
      lang = "default";
    }
    else {
      auto const it = settings.find( lang );
      if ( it == settings.end() ){
	LOG << "[internal_tokenize_line]: no settings found for language="
	    << lang << endl
	    << "using the default language instead:" << default_language << endl;
	lang = "default";
      }
    }
    if (tokDebug){
      DBG << "[internal_tokenize_line] input: line=["
	  << originput << "] (language= " << lang << ")" << endl;
    }
    UnicodeString input = originput;
    if ( doFilter ){
      input = settings[lang]->filter.filter( input );
    }
    int32_t len = input.countChar32();
    if (tokDebug){
      DBG << "[internal_tokenize_line] filtered input: line=["
	  << input << "] (" << len
	  << " unicode characters)" << endl;
    }
    const int begintokencount = tokens.size();
    if (tokDebug) {
      DBG << "[internal_tokenize_line] Tokens still in buffer: "
	  << begintokencount << endl;
    }

    bool tokenizeword = false;
    bool reset_token = false;
    //iterate over all characters
    UnicodeString word;
    StringCharacterIterator sit(input);
    long int i = 0;
    long int tok_size = 0;
    while ( sit.hasNext() ){
      UChar32 c = sit.current32();
      bool joiner = false;
      if ( c == ZWJ ){
	joiner = true;
      }
      if ( tokDebug > 8 ){
	UnicodeString s = c;
	int8_t charT = u_charType( c );
	DBG << "examine character: " << s << " type= "
	    << toString( charT  )
	    << " [" << TiCC::format_non_printable(s) << "]" << endl;
      }
      if ( reset_token ) { //reset values for new word
	reset_token = false;
	tok_size = 0;
	if ( !joiner && !is_separator(c) ){
	  word = c;
	}
	else {
	  word = "";
	}
	tokenizeword = false;
      }
      else if ( !joiner && !is_separator(c) ){
	word += c;
      }
      if ( joiner && sit.hasNext() ){
	UChar32 peek = sit.next32();
	if ( is_separator(peek) ){
	  joiner = false;
	}
	sit.previous32();
      }
      if ( is_separator(c) || joiner || i == len-1 ){
	if (tokDebug){
	  DBG << "[internal_tokenize_line] space detected, word=["
	      << word << "]" << endl;
	}
	if ( i == len-1 ) {
	  if ( joiner
	       || u_ispunct(c)
	       || u_isdigit(c)
	       || u_isquote( c, settings[lang]->quotes )
	       || u_isemo(c) ){
	    tokenizeword = true;
	  }
	}
	if ( c == '\n' && word.isEmpty() ){
	  if (tokDebug){
	    DBG << "[internal_tokenize_ine] NEW PARAGRAPH upcoming " << endl;
	  }
	  // signal that the next word starts a new Paragraph. (if its there)
	  paragraphsignal_next = true;
	}
	int expliciteosfound = -1;
	if ( word.length() >= utt_mark.length() ) {
	  expliciteosfound = word.lastIndexOf(utt_mark);

	  if (expliciteosfound != -1) { // word contains utt_mark
	    if ( tokDebug >= 2){
	      DBG << "[internal_tokenize_line] Found explicit EOS marker @"
		  << expliciteosfound << endl;
	    }
	    int eospos = tokens.size()-1;
	    if (expliciteosfound > 0) {
	      UnicodeString realword;
	      word.extract(0,expliciteosfound,realword);
	      if (tokDebug >= 2) {
		DBG << "[internal_tokenize_line] Prefix before EOS: "
		    << realword << endl;
	      }
	      tokenizeWord( realword, false, lang );
	      eospos++;
	    }
	    if ( expliciteosfound + utt_mark.length() < word.length() ){
	      UnicodeString realword;
	      word.extract( expliciteosfound+utt_mark.length(),
			    word.length() - expliciteosfound - utt_mark.length(),
			    realword );
	      if (tokDebug >= 2){
		DBG << "[internal_tokenize_line] postfix after EOS: "
		    << realword << endl;
	      }
	      tokenizeWord( realword, true, lang );
	    }
	    if ( !tokens.empty() && eospos >= 0 ) {
	      if (tokDebug >= 2){
		DBG << "[internal_tokenize_line] Assigned EOS" << endl;
	      }
	      tokens[eospos].role |= ENDOFSENTENCE;
	    }
	  }
	}
	if ( word.length() > 0
	     && expliciteosfound == -1 ) {
	  if (tokDebug >= 2){
	    DBG << "[internal_tokenize_line] Further tokenization necessary for: ["
			    << word << "]" << endl;
	  }
	  if ( tokenizeword ) {
	    tokenizeWord( word, !joiner, lang );
	  }
	  else {
	    tokenizeWord( word, !joiner, lang, type_word );
	  }
	}
	//reset values for new word
	reset_token = true;
      }
      else if ( u_ispunct(c)
		|| u_isdigit(c)
		|| u_isquote( c, settings[lang]->quotes )
		|| u_isemo(c) ){
	if (tokDebug){
	  DBG << "[internal_tokenize_line] punctuation or digit detected, word=["
	      << word << "]" << endl;
	}
	//there is punctuation or digits in this word, mark to run through tokenizer
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
    if (tokDebug >= 10){
      DBG << "tokens.size() = " << tokens.size() << endl;
      DBG << "begintokencount = " << begintokencount << endl;
      DBG << "numnew = " << numNewTokens << endl;
    }
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
	DBG << "   [tokenizeWord] Recurse Input: (" << inpLen << ") "
	    << "word=[" << input << "], type=" << assigned_type
	    << " Space=" << (space?"TRUE":"FALSE") << endl;
      }
      else {
	DBG << "   [tokenizeWord] Input: (" << inpLen << ") "
	    << "word=[" << input << "]"
	    << " Space=" << (space?"TRUE":"FALSE") << endl;      }
    }
    if ( input == utt_mark ) {
      if (tokDebug >= 2){
	DBG << "   [tokenizeWord] Found explicit EOS marker" << endl;
      }
      if (!tokens.empty()) {
	if (tokDebug >= 2){
	  DBG << "   [tokenizeWord] Assigned EOS" << endl;
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
      if ( tokDebug >= 8 ){
	DBG << " a single character: " << UnicodeString(c) << " type= "
	    << type << " [" << TiCC::format_non_printable(UnicodeString(c))
	    << "]" << endl;
      }
      if ( type == type_separator ){
	return;
      }
      else if ( type == type_unknown ){
	DBG << "Warning: Problematic character encountered:"
	    << " type=UNKNOWN value="
	    << showbase << hex << c << " ( "
	    << TiCC::format_non_printable(UnicodeString(c))
	    << ")" << endl;
      }
      if ( doPunctFilter
	   && ( type == type_punctuation || type == type_currency ||
		type == type_emoticon || type == type_picto ) ) {
	if (tokDebug >= 2 ){
	  DBG << "   [tokenizeWord] skipped PUNCTUATION ["
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
	TokenRole role = (space ? NOROLE : NOSPACE);
	if ( paragraphsignal_next ){
	  role |= NEWPARAGRAPH;
	  paragraphsignal_next = false;
	}
	Token T( type, word, role, lang );
	tokens.push_back( T );
	if (tokDebug >= 2){
	  DBG << "   [tokenizeWord] added token " << T << endl;
	}
      }
    }
    else {
      bool a_rule_matched = false;
      for ( const auto& rule : settings[lang]->rules ) {
	if ( tokDebug >= 4){
	  DBG << "\tTESTING " << rule->id << endl;
	}
	UnicodeString type = rule->id;
	//Find first matching rule
	UnicodeString pre, post;
	vector<UnicodeString> matches;
	if ( rule->matchAll( input, pre, post, matches ) ){
	  a_rule_matched = true;
	  if ( tokDebug >= 4 ){
	    DBG << "\tMATCH: " << type << endl;
	    DBG << "\tpre=  '" << pre << "'" << endl;
	    DBG << "\tpost= '" << post << "'" << endl;
	    int cnt = 0;
	    for ( const auto& m : matches ){
	      DBG << "\tmatch[" << ++cnt << "]='" << m << "'" << endl;
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
		DBG << "\trecurse, match didn't do anything new for " << input << endl;
	      }
	      TokenRole role = (space ? NOROLE : NOSPACE);
	      if ( paragraphsignal_next ){
		role |= NEWPARAGRAPH;
		paragraphsignal_next = false;
	      }
	      tokens.push_back( Token( assigned_type, input, role, lang ) );
	      return;
	    }
	    else {
	      if ( tokDebug >= 4 ){
		DBG << "\trecurse, match changes the type:"
				<< assigned_type << " to " << type << endl;
	      }
	      TokenRole role = (space ? NOROLE : NOSPACE);
	      if ( paragraphsignal_next ){
		role |= NEWPARAGRAPH;
		paragraphsignal_next = false;
	      }
	      tokens.push_back( Token( type, input, role, lang ) );
	      return;
	    }
	  }
	  if ( pre.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      DBG << "\tTOKEN pre-context (" << pre.length()
			      << "): [" << pre << "]" << endl;
	    }
	    tokenizeWord( pre, false, lang ); //pre-context, no space after
	  }
	  if ( matches.size() > 0 ){
	    int max = matches.size();
	    if ( tokDebug >= 4 ){
	      DBG << "\tTOKEN match #=" << matches.size() << endl;
	    }
	    for ( int m=0; m < max; ++m ){
	      if ( tokDebug >= 4 ){
		DBG << "\tTOKEN match[" << m << "] = " << matches[m]
		    << " Space=" << (space?"TRUE":"FALSE") << endl;
	      }
	      if ( doPunctFilter
		   && (&rule->id)->startsWith("PUNCTUATION") ){
		if (tokDebug >= 2 ){
		  DBG << "   [tokenizeWord] skipped PUNCTUATION ["
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
		  TokenRole role = (internal_space ? NOROLE : NOSPACE);
		  if ( paragraphsignal_next ){
		    role |= NEWPARAGRAPH;
		    paragraphsignal_next = false;
		  }
		  tokens.push_back( Token( type, word, role, lang ) );
		}
		else {
		  if ( recurse ){
		    TokenRole role = (internal_space ? NOROLE : NOSPACE);
		    if ( paragraphsignal_next ){
		      role |= NEWPARAGRAPH;
		      paragraphsignal_next = false;
		    }
		    tokens.push_back( Token( type, word, role, lang ) );
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
	    DBG << "\tPANIC there's no match" << endl;
	  }
	  if ( post.length() > 0 ){
	    if ( tokDebug >= 4 ){
	      DBG << "\tTOKEN post-context (" << post.length()
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
	  DBG << "\tthere's no match at all" << endl;
	}
	TokenRole role = (space ? NOROLE : NOSPACE);
	if ( paragraphsignal_next ){
	  role |= NEWPARAGRAPH;
	  paragraphsignal_next = false;
	}
	tokens.push_back( Token( assigned_type, input, role, lang ) );
      }
    }
  }

  string TokenizerClass::get_data_version() const {
    return UCTODATA_VERSION;
  }

  bool TokenizerClass::init( const string& fname, const string& tname ){
    if ( tokDebug ){
      DBG << "Initiating tokenizer..." << endl;
    }
    data_version = get_data_version();
    Setting *set = new Setting();
    if ( !set->read( fname, tname, tokDebug, theErrLog, theDbgLog ) ){
      LOG << "Cannot read Tokenizer settingsfile " << fname << endl;
      LOG << "Unsupported language? (Did you install the uctodata package?)"
	  << endl;
      delete set;
      return false;
    }
    else {
      settings["default"] = set;
      default_language = "default";
      auto pos = fname.find(config_prefix());
      if ( pos != string::npos ){
	default_language = fname.substr(pos+10);
	settings[default_language] = set;
      }
      else if ( xmlout ){
	LOG << " unable to determine a language. cannot proceed" << endl;
	return false;
      }
    }
    if ( tokDebug ){
      DBG << "effective rules: " << endl;
      for ( size_t i=0; i < set->rules.size(); ++i ){
	DBG << "rule " << i << " " << *(set->rules[i]) << endl;
      }
      DBG << "EOS markers: " << set->eosmarkers << endl;
      DBG << "Quotations: " << set->quotes << endl;
      try {
	DBG << "Filter: " << set->filter << endl;
      }
      catch (...){
      }
    }
    if ( doDetectLang ){
      return initialize_textcat();
    }
    return true;
  }

  bool TokenizerClass::init( const vector<string>& languages,
			     const string& tname ){
    if ( tokDebug > 0 ){
      DBG << "Initiating tokenizer from language list..." << endl;
    }
    data_version = get_data_version();
    // first a quick check
    set<string> available = Setting::installed_languages();
    set<string> rejected;
    for ( const auto& lang : languages ){
      if ( lang == "und" ){
	continue;
      }
      if ( available.find( lang ) == available.end() ){
	rejected.insert(lang);
      }
    }
    if ( !rejected.empty() ){
      LOG << "Unsupported languages: " << rejected << endl;
      LOG << "(Did you install the uctodata package?)" << endl;
      LOG << "terminating..." << endl;
      return false;
    }
    Setting *default_set = 0;
    for ( const auto& lang : languages ){
      if ( lang == "und" ){
	settings["und"] = 0;
	und_language = true;
	continue;
      }
      if ( tokDebug > 0 ){
	DBG << "init language=" << lang << endl;
      }
      string fname = config_prefix() + lang;
      Setting *set = new Setting();
      string add;
      if ( default_set == 0 ){
	add = tname;
      }
      if ( !set->read( fname, add, tokDebug, theErrLog, theDbgLog ) ){
	LOG << "problem reading datafile for language: " << lang << endl;
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
      cerr << "ucto: No useful settingsfile(s) could be found (initiating from language list: " << languages << ")" << endl;
      return false;
    }
    if ( doDetectLang ){
      return initialize_textcat();
    }
    return true;
  }

  string get_language( const vector<Token>& tv ){
    // examine the assigned languages of ALL tokens.
    // they should all be the same
    // assign that value
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

  bool TokenizerClass::get_setting_info( const std::string& language,
					 std::string& set_file,
					 std::string& version ) const {
    set_file.clear();
    version.clear();
    auto const& it = settings.find( language );
    if ( it == settings.end() ){
      return false;
    }
    else {
      set_file = it->second->set_file;
      version = it->second->version;
      return true;
    }
  }

} //namespace Tokenizer
