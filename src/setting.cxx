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

#include <unistd.h>
#include <pwd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <functional>  // for std::plus
#include <numeric>     // for std::accumulate
#include "config.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "ucto/setting.h"

using namespace std;
using TiCC::operator<<;

#define LOG *TiCC::Log(theErrLog)
#define DBG *TiCC::Log(theDbgLog)

#ifndef UCTODATA_DIR
#define UCTODATA_DIR string(SYSCONF_PATH) + "/ucto/"
#endif

const char *homedir = getenv("HOME") ? getenv("HOME") : getpwuid(getuid())->pw_dir; //never NULL
const char *xdgconfighome = getenv("XDG_CONFIG_HOME"); //may be NULL
const string localConfigDir = ((xdgconfighome != NULL) ? string(xdgconfighome) : string(homedir) + "/.config") + "/ucto/";

namespace Tokenizer {

  using namespace icu;
  using TiCC::operator<<;

  const string _config_prefix = "tokconfig-"; // the prefix we use for all configfiles
  const std::string config_prefix() { return _config_prefix; };
  string defaultConfigDir = UCTODATA_DIR;

  enum ConfigMode { NONE, RULES, ABBREVIATIONS, ATTACHEDPREFIXES,
		    ATTACHEDSUFFIXES, PREFIXES, SUFFIXES, TOKENS, UNITS,
		    ORDINALS, EOSMARKERS, QUOTES, CURRENCY,
		    FILTER, RULEORDER, METARULES };

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
    else if (line == "[CURRENCY]") {
      mode = CURRENCY;
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
    else {
      mode = NONE;
    }
    return mode;
  }

  class uConfigError: public std::invalid_argument {
  public:
    uConfigError( const string& s, const string& f ):
      invalid_argument( "ucto: " + s + " (" + f + ")"  ){};
    uConfigError( const UnicodeString& us, const string& f ):
      uConfigError( TiCC::UnicodeToUTF8(us), f ){};
  };

  class uLogicError: public std::logic_error {
  public:
    explicit uLogicError( const string& s ): logic_error( "ucto: logic error:" + s ){};
  };

  ostream& operator<<( ostream& os, const Quoting& q ){
    for( const auto& quote : q._quotes ){
      os << quote.openQuote << "\t" << quote.closeQuote << endl;
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
    _quotes.push_back( quote );
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
    auto res = find_if( _quotes.begin(),
			_quotes.end(),
			[q]( const QuotePair& qp){ return qp.openQuote.indexOf(q) >=0; } );
    if ( res != _quotes.end() ){
      return res->closeQuote;
    }
    else {
      return "";
    }
  }

  UnicodeString Quoting::lookupClose( const UnicodeString &q ) const {
    UnicodeString res;
    for ( const auto& quote : _quotes ){
      if ( quote.closeQuote.indexOf(q) >= 0 )
	return quote.openQuote;
    }
    return "";
  }

  Rule::~Rule() {
    delete regexp;
  }

  Rule::Rule( const UnicodeString& _id, const UnicodeString& _pattern):
    id(_id), pattern(_pattern) {
    regexp = new TiCC::UnicodeRegexMatcher( pattern, id );
  }

  ostream& operator<< (std::ostream& os, const Rule& r ){
    if ( r.regexp ){
      os << r.id << "=\"" << r.regexp->Pattern() << "\"";
    }
    else
      os << r.id  << "=NULL";
    return os;
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

  Setting::~Setting(){
    for ( const auto rule : rules ) {
      delete rule;
    }
    rulesmap.clear();
  }

  set<string> Setting::installed_languages() {
    // static function
    // we only return 'languages' which are installed as '_config_prefix*'
    //
    vector<string> files;
    if ( TiCC::isDir(localConfigDir) ) {
      auto const localfiles= TiCC::searchFilesMatch( localConfigDir,
						     config_prefix()+"*" );
      files.insert(files.end(), localfiles.begin(), localfiles.end());
    }
    if ( TiCC::isDir(defaultConfigDir) ) {
      auto const globalfiles = TiCC::searchFilesMatch( defaultConfigDir,
						       config_prefix()+"*" );
      files.insert(files.end(), globalfiles.begin(), globalfiles.end());
    }
    set<string> result;
    for ( auto const& f : files ){
      string base = TiCC::basename(f);
      size_t pos = base.find(config_prefix());
      if ( pos == 0 ){
	string lang = base.substr( 10 );
	result.insert( lang );
      }
    }
    return result;
  }

  bool Setting::read_rules( const string& fname ){
    if ( tokDebug > 0 ){
      DBG << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline( f, rawline ) ){
	UnicodeString line = TiCC::UnicodeFromUTF8(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    DBG << "include line = " << rawline << endl;
	  }
	  const int splitpoint = line.indexOf("=");
	  if ( splitpoint < 0 ){
	    throw uConfigError( "invalid RULES entry: " + line,
				fname );
	  }
	  UnicodeString id = UnicodeString( line, 0,splitpoint);
	  UnicodeString pat = UnicodeString( line, splitpoint+1);
	  rulesmap[id] = new Rule( id, pat );
	}
      }
    }
    return true;
  }

  bool Setting::read_filters( const string& fname ){
    if ( tokDebug > 0 ){
      DBG << "%include " << fname << endl;
    }
    return filter.fill( fname );
  }

  bool Setting::read_quotes( const string& fname ){
    if ( tokDebug > 0 ){
      DBG << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline( f, rawline ) ){
	UnicodeString line = TiCC::UnicodeFromUTF8(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    DBG << "include line = " << rawline << endl;
	  }
	  int splitpoint = line.indexOf(" ");
	  if ( splitpoint == -1 ){
	    splitpoint = line.indexOf("\t");
	  }
	  if ( splitpoint == -1 ){
	    throw uConfigError( "invalid QUOTES entry: " + line
				+ " (missing whitespace)",
				fname );
	  }
	  UnicodeString open = UnicodeString( line, 0,splitpoint);
	  UnicodeString close = UnicodeString( line, splitpoint+1);
	  open = open.trim().unescape();
	  close = close.trim().unescape();
	  if ( open.isEmpty() || close.isEmpty() ){
	    throw uConfigError( "invalid QUOTES entry: " + line, fname );
	  }
	  else {
	    quotes.add( open, close );
	  }
	}
      }
    }
    return true;
  }

  bool Setting::read_eosmarkers( const string& fname ){
    if ( tokDebug > 0 ){
      DBG << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline( f, rawline ) ){
	UnicodeString line = TiCC::UnicodeFromUTF8(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    DBG << "include line = " << rawline << endl;
	  }
	  if ( ( line.startsWith("\\u") && line.length() == 6 ) ||
	       ( line.startsWith("\\U") && line.length() == 10 ) ){
	    UnicodeString uit = line.unescape();
	    if ( uit.isEmpty() ){
	      throw uConfigError( "Invalid EOSMARKERS entry: " + line, fname );
	    }
	    eosmarkers += uit;
	  }
	}
      }
    }
    return true;
  }

  UnicodeString escape_regex( const UnicodeString& entry ){
    UnicodeString result;
    for ( int i=0; i < entry.length(); ++i ){
      switch ( entry[i] ){
      case '?':
      case '^':
      case '$':
      case '[':
      case ']':
      case '(':
      case ')':
      case '{':
      case '}':
      case '*':
      case '.':
      case '+':
      case '|':
      case '-':
	if ( i == 0 || entry[i-1] != '\\' ){
	  // not escaped
	  result += "\\";
	}
	[[fallthrough]];
      default:
	result += entry[i];
      }
    }
    return result;
  }

  bool Setting::read_abbreviations( const string& fname,
				    UnicodeString& abbreviations ){
    if ( tokDebug > 0 ){
      DBG << "%include " << fname << endl;
    }
    ifstream f( fname );
    if ( !f ){
      return false;
    }
    else {
      string rawline;
      while ( getline( f, rawline ) ){
	UnicodeString line = TiCC::UnicodeFromUTF8(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if ( tokDebug >= 5 ){
	    DBG << "include line = " << rawline << endl;
	  }
	  line = escape_regex( line );
	  if ( !abbreviations.isEmpty()){
	    abbreviations += '|';
	  }
	  abbreviations += line;
	}
      }
    }
    return true;
  }

  void Setting::add_rule( const UnicodeString& name,
			  const vector<UnicodeString>& parts ){
    UnicodeString pat = std::accumulate( parts.begin(), parts.end(),
					 UnicodeString(),
					 std::plus<UnicodeString>() );
    rulesmap[name] = new Rule( name, pat );
  }

  void Setting::sort_rules( map<UnicodeString, Rule *>& rulesmap,
			    const vector<UnicodeString>& sort ){
    // DBG << "rules voor sort : " << endl;
    // for ( size_t i=0; i < rules.size(); ++i ){
    //   DBG << "rule " << i << " " << *rules[i] << endl;
    // }
    int index = 0;
    if ( !sort.empty() ){
      for ( auto const& id : sort ){
	auto it = rulesmap.find( id );
	if ( it != rulesmap.end() ){
	  rules.push_back( it->second );
	  rules_index[id] = ++index;
	  rulesmap.erase( it );
	}
	else {
	  LOG << set_file << ": RULE-ORDER specified for undefined RULE '"
	      << id << "'" << endl;
	}
      }
      for ( auto const& it : rulesmap ){
	LOG << set_file << ": No RULE-ORDER specified for RULE '"
	    << it.first << "' (put at end)." << endl;
	rules.push_back( it.second );
	rules_index[it.first] = ++index;
      }
    }
    else {
      for ( auto const& it : rulesmap ){
	rules.push_back( it.second );
	rules_index[it.first] = ++index;
      }
    }
    // DBG << "rules NA sort : " << endl;
    // for ( size_t i=0; i < result.size(); ++i ){
    //   DBG << "rule " << i << " " << *result[i] << endl;
    // }
  }

  string get_filename( const string& name , const string customConfigDir = ""){
    string result;
    if ( TiCC::isFile( name ) ){
      result = name;
    }
    else {
      if (!customConfigDir.empty()) {
        result = customConfigDir + name;
        if (TiCC::isFile(result)) {
          return result;
        }
      }
      result = localConfigDir + name;
      if ( !TiCC::isFile( result ) ){
          result = defaultConfigDir + name;
          if ( !TiCC::isFile( result ) ){
            result.clear();
          }
      }
    }
    return result;
  }

  void addOrder( vector<UnicodeString>& order,
		 map<UnicodeString,int>& reverse_order,
		 int& index,
		 UnicodeString &line,
		 const string& fn ){
    try {
      TiCC::UnicodeRegexMatcher m( "\\s+" );
      vector<UnicodeString> usv;
      m.split( line, usv );
      for ( const auto& us : usv  ){
	if ( reverse_order.find( us ) != reverse_order.end() ){
	  throw uConfigError( "multiple entry " + us + " in RULE-ORDER", fn );
	}
	order.push_back( us );
	reverse_order[us] = ++index;
      }
    }
    catch ( const uConfigError& ){
      throw;
    }
    catch ( exception& e ){
      throw uConfigError( "problem in line:" + line, "" );
    }
  }

  void split( const string& version, int& major, int& minor, string& sub ){
    vector<string> parts = TiCC::split_at( version, "." );
    size_t num = parts.size();
    major = 0;
    minor = 0;
    sub.clear();
    if ( num == 0 ){
      sub = version;
    }
    else if ( num == 1 ){
      if ( !TiCC::stringTo( parts[0], major ) ){
	sub = version;
      }
    }
    else if ( num == 2 ){
      if ( !TiCC::stringTo( parts[0], major ) ){
	sub = version;
      }
      else if ( !TiCC::stringTo( parts[1], minor ) ){
	sub = parts[1];
      }
    }
    else { // num > 2
      if ( !TiCC::stringTo( parts[0], major ) ){
	sub = version;
      }
      else if ( !TiCC::stringTo( parts[1], minor ) ){
	sub = parts[1];
      }
      else {
	for ( size_t i=2; i < num; ++i ){
	  sub += parts[i];
	  if ( i < num-1 )
	    sub += ".";
	}
      }
    }
  }

  UnicodeString substitute_macros( const UnicodeString& in,
				   const map<UnicodeString,UnicodeString>& macros ){
    UnicodeString result = in;
    for ( const auto& it : macros ){
      result.findAndReplace( it.first, it.second );
    }
    return result;
  }

  bool Setting::read( const string& settings_name,
		      const string& add_tokens,
		      int dbg,
		      TiCC::LogStream* ls,
		      TiCC::LogStream* ds ) {
    tokDebug = dbg;
    theErrLog = ls;
    theDbgLog = ds;
    splitter = "%";
    map<ConfigMode, UnicodeString> patterns = { { ABBREVIATIONS, "" },
						{ TOKENS, "" },
						{ PREFIXES, "" },
						{ SUFFIXES, "" },
						{ ATTACHEDPREFIXES, "" },
						{ ATTACHEDSUFFIXES, "" },
						{ UNITS, "" },
						{ ORDINALS, "" } };
    string conffile = get_filename( settings_name );

    string customconfdir = TiCC::dirname(settings_name);
    if (!TiCC::match_back( customconfdir, "/")) {
        customconfdir += "/";
    }
    if (customconfdir == "./") {
        customconfdir.clear();
    }

    if ( !TiCC::isFile( conffile ) ){
      LOG << "Unable to open configfile: " << conffile << endl;
      return false;
    }
    if ( !add_tokens.empty() && !TiCC::isFile( add_tokens ) ){
      LOG << "Unable to open additional tokens file: " << add_tokens << endl;
      return false;
    }
    ifstream f( conffile );
    if ( !f ){
      return false;
    }
    else {
      vector<string> meta_rules;
      vector<UnicodeString> rules_order;
      ConfigMode mode = NONE;
      set_file = settings_name;
      if ( tokDebug ){
	DBG << "config file=" << conffile << endl;
      }
      int rule_count = 0;
      string rawline;
      while ( getline( f, rawline ) ){
	if ( rawline.find( "%include" ) != string::npos ){
	  string file = rawline.substr( 9 );
	  switch ( mode ){
	  case RULES: {
	    if ( !TiCC::match_back( file, ".rule" ) ){
	      file += ".rule";
	    }
	    file = get_filename( file, customconfdir);
	    if ( !read_rules( file ) ){
	      throw uConfigError( "'" + rawline + "' failed", set_file );
	    }
	  }
	    break;
	  case FILTER:{
	    if ( !TiCC::match_back( file, ".filter" ) ){
	      file += ".filter";
	    }
	    file = get_filename( file, customconfdir );
	    if ( !read_filters( file ) ){
	      throw uConfigError( "'" + rawline + "' failed", set_file );
	    }
	  }
	    break;
	  case QUOTES:{
	    if ( !TiCC::match_back( file, ".quote" ) ){
	      file += ".quote";
	    }
	    file = get_filename( file, customconfdir );
	    if ( !read_quotes( file ) ){
	      throw uConfigError( "'" + rawline + "' failed", set_file );
	    }
	  }
	    break;
	  case EOSMARKERS:{
	    if ( !TiCC::match_back( file, ".eos" ) ){
	      file += ".eos";
	    }
	    file = get_filename( file, customconfdir);
	    if ( !read_eosmarkers( file ) ){
	      throw uConfigError( "'" + rawline + "' failed", set_file );
	    }
	  }
	    break;
	  case ABBREVIATIONS:{
	    if ( !TiCC::match_back( file, ".abr" ) ){
	      file += ".abr";
	    }
	    file = get_filename( file, customconfdir );
	    if ( !read_abbreviations( file, patterns[ABBREVIATIONS] ) ){
	      throw uConfigError( "'" + rawline + "' failed", set_file );
	    }
	  }
	    break;
	  default:
	    throw uConfigError( string("%include not implemented for this section"),
				set_file );
	  }
	  continue;
	}
	else if ( rawline.find( "%define" ) != string::npos ){
	  string def = rawline.substr( 8 );
	  vector<string> parts = TiCC::split_at_first_of( def, " \t", 2 );
	  if ( parts.size() < 2 ){
	    throw uConfigError( "invalid %define: " + rawline, set_file );
	  }
	  UnicodeString macro = TiCC::UnicodeFromUTF8(splitter)
	    + TiCC::UnicodeFromUTF8(parts[0]) + TiCC::UnicodeFromUTF8(splitter);
	  macros[macro] = TiCC::UnicodeFromUTF8(parts[1]);
	  continue;
	}
	else if ( rawline.find( "SPLITTER=" ) != string::npos ){
	  string local_splitter = rawline.substr( 9 );
	  if ( local_splitter.empty() ) {
	    throw uConfigError( "invalid SPLITTER value in: " + rawline,
				set_file );
	  }
	  if ( local_splitter[0] == '"'
	       && local_splitter[local_splitter.length()-1] == '"' ){
	    local_splitter = local_splitter.substr(1,local_splitter.length()-2);
	  }
	  if ( tokDebug > 5 ){
	    DBG << "SET SPLITTER: '" << local_splitter << "'" << endl;
	  }
	  if ( local_splitter != splitter ){
	    DBG << "updating splitter to: '" << local_splitter << "'" << endl;
	  }
	  splitter = local_splitter;
	  continue;
	}

	UnicodeString line = TiCC::UnicodeFromUTF8(rawline);
	line.trim();
	if ((line.length() > 0) && (line[0] != '#')) {
	  if (line[0] == '[') {
	    mode = getMode( line );
	  }
	  else {
	    if ( line[0] == '\\' && line.length() > 1 && line[1] == '[' ){
	      line = UnicodeString( line, 1 );
	    }
	    line = substitute_macros( line, macros );
	    switch( mode ){
	    case RULES: {
	      const int splitpoint = line.indexOf("=");
	      if ( splitpoint < 0 ){
		throw uConfigError( "invalid RULES entry: " + line,
				    set_file );
	      }
	      UnicodeString id = UnicodeString( line, 0,splitpoint);
	      UnicodeString pat = UnicodeString( line, splitpoint+1);
	      rulesmap[id] = new Rule( id, pat );
	    }
	      break;
	    case RULEORDER:
	      addOrder( rules_order, rules_index,
			rule_count, line, set_file );
	      break;
	    case METARULES:
	      meta_rules.push_back( TiCC::UnicodeToUTF8(line) );
	      break;
	    case ABBREVIATIONS:
	    case ATTACHEDPREFIXES:
	    case ATTACHEDSUFFIXES:
	    case PREFIXES:
	    case SUFFIXES:
	    case TOKENS:
	    case CURRENCY:
	    case UNITS:
	    case ORDINALS:
	      if ( !patterns[mode].isEmpty() )
		patterns[mode] += '|';
	      patterns[mode] += line;
	      break;
	    case EOSMARKERS:
	      if ( ( line.startsWith("\\u") && line.length() == 6 ) ||
		   ( line.startsWith("\\U") && line.length() == 10 ) ){
		UnicodeString uit = line.unescape();
		if ( uit.isEmpty() ){
		  throw uConfigError( "Invalid EOSMARKERS entry: " + line,
				      set_file );
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
				    + " (missing whitespace)",
				    set_file );
	      }
	      UnicodeString open = UnicodeString( line, 0,splitpoint);
	      UnicodeString close = UnicodeString( line, splitpoint+1);
	      open = open.trim().unescape();
	      close = close.trim().unescape();
	      if ( open.isEmpty() || close.isEmpty() ){
		throw uConfigError( "invalid QUOTES entry: " + line,
				    set_file );
	      }
	      else {
		quotes.add( open, close );
	      }
	    }
	      break;
	    case FILTER:
	      filter.add( line );
	      break;
	    case NONE: {
	      vector<string> parts = TiCC::split_at( rawline, "=" );
	      if ( parts.size() == 2 ) {
		if ( parts[0] == "version" ){
		  version = parts[1];
		}
	      }
	    }
	      break;
	    default:
	      throw uLogicError( "unhandled case in switch" );
	    }
	  }
	}
      }

      // set reasonable defaults for those items that are NOT set
      // in the configfile
      if ( eosmarkers.length() == 0 ){
	eosmarkers = ".!?";
      }
      if ( quotes.empty() ){
	quotes.add( '"', '"' );
	quotes.add( "‘", "’" );
	quotes.add( "“„‟", "”" );
      }

      if ( !add_tokens.empty() ){
	ifstream adt( add_tokens );
	string line;
	while ( getline( adt, line ) ){
	  UnicodeString entry = TiCC::UnicodeFromUTF8(line);
	  entry = escape_regex( entry );
	  if ( !entry.isEmpty() ){
	    if ( !patterns[TOKENS].isEmpty() ){
	      patterns[TOKENS] += '|';
	    }
	    patterns[TOKENS] += entry;
	  }
	}
      }
      // Create Rules for every pattern that is set
      // first the meta rules...
      for ( const auto& mr : meta_rules ){
	string::size_type pos = mr.find( "=" );
	if ( pos == string::npos ){
	  throw uConfigError( "invalid entry in META-RULES: " + mr,
			      set_file );
	}
	string nam = TiCC::trim( mr.substr( 0, pos ) );
	if ( nam == "SPLITTER" ){
	  string local_splitter = mr.substr( pos+1 );
	  if ( local_splitter.empty() ) {
	    throw uConfigError( "invalid SPLITTER value in META-RULES: " + mr,
				set_file );
	  }
	  if ( local_splitter[0] == '"'
	       && local_splitter[local_splitter.length()-1] == '"' ){
	    local_splitter = local_splitter.substr(1,local_splitter.length()-2);
	  }
	  if ( tokDebug > 5 ){
	    DBG << "SET SPLITTER: '" << local_splitter << "'" << endl;
	  }
	  if ( local_splitter != splitter ){
	    LOG << "updating splitter to: '" << local_splitter << "'" << endl;
	  }
	  splitter = local_splitter;
	  continue;
	}
	UnicodeString name = TiCC::UnicodeFromUTF8( nam );
	string rule = mr.substr( pos+1 );
	if ( tokDebug > 5 ){
	  DBG << "SPLIT using: '" << splitter << "'" << endl;
	}
	vector<string> parts = TiCC::split_at( rule, splitter );
	for ( auto& str : parts ){
	  str = TiCC::trim( str );
	}
	vector<UnicodeString> new_parts;
	vector<UnicodeString> undef_parts;
	bool skip_rule = false;
	for ( const auto& part : parts ){
	  UnicodeString meta = TiCC::UnicodeFromUTF8( part );
	  ConfigMode local_mode = getMode( "[" + meta + "]" );
	  switch ( local_mode ){
	  case ORDINALS:
	  case ABBREVIATIONS:
	  case TOKENS:
	  case ATTACHEDPREFIXES:
	  case ATTACHEDSUFFIXES:
	  case UNITS:
	  case CURRENCY:
	  case PREFIXES:
	  case SUFFIXES:
	    if ( !patterns[local_mode].isEmpty()){
	      UnicodeString val = substitute_macros( patterns[local_mode],
						     macros );
	      new_parts.push_back( val );
	    }
	    else {
	      undef_parts.push_back( meta );
	      skip_rule = true;
	    }
	    break;
	  case NONE:
	  default:
	    new_parts.push_back( substitute_macros( TiCC::UnicodeFromUTF8(part),
						    macros ) );
	    break;
	  }
	}
	if ( skip_rule ){
	  using TiCC::operator<<;
	  LOG << set_file << ": skipping META rule: '" << name
	      << "', it mentions unknown pattern: '"
	      << undef_parts <<"'" << endl;
	}
	else {
	  add_rule( name, new_parts );
	}
      }
      sort_rules( rulesmap, rules_order );
    }
    int major = -1;
    int minor = -1;
    if ( !version.empty() ){
      string sub;
      split( version, major, minor, sub );
      if ( tokDebug ){
	DBG << set_file << ": version=" << version << endl;
      }
    }
    if ( major < 0 || minor < 2 ){
      if ( version.empty() ){
	LOG << "WARNING: your datafile for '" + set_file
	    << "' is missing a version number" << endl;
	LOG << "         Did you install uctodata version >=0.2 ?" << endl;
	LOG << "         or do you use your own setingsfile? Then please add a version number." << endl;
      }
      else {
	LOG << "WARNING: your datafile '" + set_file
	    << "' has version: " << version << endl;
	LOG << "         for best results, you should a file with version >=0.2 " << endl;
      }
    }
    if ( tokDebug ){
      DBG << "effective rules: " << endl;
      for ( size_t i=0; i < rules.size(); ++i ){
	DBG << "rule " << i << " " << *rules[i] << endl;
      }
      DBG << "EOS markers: " << eosmarkers << endl;
      DBG << "Quotations: " << quotes << endl;
      try {
	DBG << "Filter: " << filter << endl;
      }
      catch (...){
      }
    }
    return true;
  }

}//namespace
