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

#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "config.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"
#include "libfolia/folia.h"
#include "ucto/unicode.h"
#include "ucto/setting.h"

using namespace std;
using namespace TiCC;

#define LOG *Log(theErrLog)

namespace Tokenizer {
  string defaultConfigDir = string(SYSCONF_PATH) + "/ucto/";

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
    uConfigError( const string& s ): invalid_argument( "ucto: config file:" + s ){};
    uConfigError( const UnicodeString& us ): invalid_argument( "ucto: config file:" + folia::UnicodeToUTF8(us) ){};
  };

  class uLogicError: public std::logic_error {
  public:
    uLogicError( const string& s ): logic_error( "ucto: logic error:" + s ){};
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
    for ( const auto& quote : _quotes ){
      if ( quote.openQuote.indexOf(q) >=0 )
	return quote.closeQuote;
    }
    return "";
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
    regexp = new UnicodeRegexMatcher( pattern, id );
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
    delete theErrLog;
  }
  bool Setting::readrules( const string& fname ){
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

  bool Setting::readfilters( const string& fname ){
    if ( tokDebug > 0 ){
      *theErrLog << "%include " << fname << endl;
    }
    return filter.fill( fname );
  }

  bool Setting::readquotes( const string& fname ){
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

  bool Setting::readeosmarkers( const string& fname ){
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

  bool Setting::readabbreviations( const string& fname,
				   UnicodeString& abbreviations ){
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

  void Setting::add_rule( const UnicodeString& name,
			  const vector<UnicodeString>& parts ){
    UnicodeString pat;
    for ( auto const& part : parts ){
      pat += part;
    }
    rulesmap[name] = new Rule( name, pat );
  }

  void Setting::sortRules( map<UnicodeString, Rule *>& rulesmap,
			   const vector<UnicodeString>& sort ){
    // LOG << "rules voor sort : " << endl;
    // for ( size_t i=0; i < rules.size(); ++i ){
    //   LOG << "rule " << i << " " << *rules[i] << endl;
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
	  LOG << "RULE-ORDER specified for undefined RULE '"
			  << id << "'" << endl;
	}
      }
      for ( auto const& it : rulesmap ){
	LOG << "No RULE-ORDER specified for RULE '"
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
    // LOG << "rules NA sort : " << endl;
    // for ( size_t i=0; i < result.size(); ++i ){
    //   LOG << "rule " << i << " " << *result[i] << endl;
    // }
  }

  string get_filename( const string& name ){
    string result;
    if ( TiCC::isFile( name ) ){
      result = name;
      if ( name.find_first_of( "/" ) != string::npos ){
	// name seems a relative or absolute path
	string::size_type pos = name.rfind("/");
      }
    }
    else {
      result = defaultConfigDir + name;
      if ( !TiCC::isFile( result ) ){
	result.clear();
      }
    }
    return result;
  }


  void addOrder( vector<UnicodeString>& order,
		 map<UnicodeString,int>& reverse_order,
		 int& index,
		 UnicodeString &line ){
    try {
      UnicodeRegexMatcher m( "\\s+" );
      vector<UnicodeString> usv;
      m.split( line, usv );
      for ( const auto& us : usv  ){
	if ( reverse_order.find( us ) != reverse_order.end() ){
	  cerr << "multiple entry " << us << " in RULE-ORDER" << endl;
	  exit( EXIT_FAILURE );
	}
	order.push_back( us );
	reverse_order[us] = ++index;
      }
    }
    catch ( exception& e ){
      throw uConfigError( "problem in line:" + line );
    }
  }

  void split( const string& version, int& major, int& minor, string& sub ){
    vector<string> parts;
    size_t num = split_at( version, parts, "." );
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
    else if ( num > 2 ){
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

  bool Setting::read( const string& settings_name,
		      int dbg, LogStream* ls ) {
    tokDebug = dbg;
    theErrLog = ls;
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
    int rule_count = 0;
    vector<string> meta_rules;

    string conffile = get_filename( settings_name );

    ifstream f( conffile );
    if ( f ){
      if ( tokDebug ){
	LOG << "config file=" << conffile << endl;
      }
      string rawline;
      while ( getline(f,rawline) ){
	if ( rawline.find( "%include" ) != string::npos ){
	  string file = rawline.substr( 9 );
	  switch ( mode ){
	  case RULES: {
	    file += ".rule";
	    file = get_filename( file );
	    if ( !readrules( file ) ){
	      throw uConfigError( "'" + rawline + "' failed" );
	    }
	  }
	    break;
	  case FILTER:{
	    file += ".filter";
	    file = get_filename( file );
	    if ( !readfilters( file ) ){
	      throw uConfigError( "'" + rawline + "' failed" );
	    }
	  }
	    break;
	  case QUOTES:{
	    file += ".quote";
	    file = get_filename( file );
	    if ( !readquotes( file ) ){
	      throw uConfigError( "'" + rawline + "' failed" );
	    }
	  }
	    break;
	  case EOSMARKERS:{
	    file += ".eos";
	    file = get_filename( file );
	    if ( !readeosmarkers( file ) ){
	      throw uConfigError( "'" + rawline + "' failed" );
	    }
	  }
	    break;
	  case ABBREVIATIONS:{
	    file += ".abr";
	    file = get_filename( file );
	    if ( !readabbreviations( file, pattern[ABBREVIATIONS] ) ){
	      throw uConfigError( "'" + rawline + "' failed" );
	    }
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
	      addOrder( rules_order, rules_index, rule_count, line );
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
	    case CURRENCY:
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
	    case NONE: {
	      vector<string> parts;
	      split_at( rawline, parts, "=" );
	      if ( parts.size() == 2 ) {
		if ( parts[0] == "version" ){
		  version = parts[1];
		}
	      }
	    }
	      break;
	    default:
	      throw uLogicError("unhandled case in switch");
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
	    LOG << "SET SPLIT: '" << split << "'" << endl;
	  }
	  continue;
	}
	UnicodeString name = folia::UTF8ToUnicode( nam );
	string rule = mr.substr( pos+1 );
	if ( tokDebug > 5 ){
	  LOG << "SPLIT using: '" << split << "'" << endl;
	}
	vector<string> parts;
	TiCC::split_at( rule, parts, split );
	// if ( num != 3 ){
	// 	throw uConfigError( "invalid entry in META-RULES: " + mr + " 3 parts expected" );
	// }
	for ( auto& str : parts ){
	  str = TiCC::trim( str );
	}
	vector<UnicodeString> new_parts;
	bool skip_rule = false;
	for ( const auto& part : parts ){
	  UnicodeString meta = folia::UTF8ToUnicode( part );
	  ConfigMode mode = getMode( "[" + meta + "]" );
	  switch ( mode ){
	  case ORDINALS:
	  case ABBREVIATIONS:
	  case TOKENS:
	  case ATTACHEDPREFIXES:
	  case ATTACHEDSUFFIXES:
	  case UNITS:
	  case CURRENCY:
	  case PREFIXES:
	  case SUFFIXES:
	    if ( !pattern[mode].isEmpty()){
	      new_parts.push_back( pattern[mode] );
	    }
	    else {
	      skip_rule = true;
	    }
	    break;
	  case NONE:
	  default:
	    new_parts.push_back( folia::UTF8ToUnicode(part) );
	    break;
	  }
	}
	if ( skip_rule ){
	  LOG << "skipping META rule: '" << name << "'" << endl;
	}
	else {
	  add_rule( name, new_parts );
	}
      }
      sortRules( rulesmap, rules_order );
    }
    else {
      return false;
    }
    int major = -1;
    int minor = -1;
    string sub;
    if ( !version.empty() ){
      split( version, major, minor, sub );
      LOG << "datafile version=" << version << endl;
    }
    if ( major < 0 || minor < 2 ){
      if ( version.empty() ){
	LOG << "WARNING: your datafile '" + settings_name
	    << "' is missing a version number" << endl;
	LOG << "         Did you install uctodata version >=0.2 ?" << endl;
	LOG << "         or do you use your own setingsfile? Then please add a version number." << endl;
      }
      else {
	LOG << "WARNING: your datafile '" + settings_name
	    << "' has version: " << version << endl;
	LOG << "         for best results, you should a file with version >=0.2 " << endl;
      }
    }
    set_file = settings_name;
    if ( tokDebug ){
      LOG << "effective rules: " << endl;
      for ( size_t i=0; i < rules.size(); ++i ){
	LOG << "rule " << i << " " << *rules[i] << endl;
      }
      LOG << "EOS markers: " << eosmarkers << endl;
      LOG << "Quotations: " << quotes << endl;
      LOG << "Filter: " << filter << endl;
    }
    return true;
  }

}//namespace
