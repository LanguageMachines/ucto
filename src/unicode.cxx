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

#include <string>
#include <ostream>
#include <fstream>
#include <map>
#include <stdexcept>
#include "unicode/ustream.h"
#include "unicode/regex.h"
#include "unicode/ucnv.h"
#include "unicode/schriter.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
#include "ucto/unicode.h"

using namespace std;

namespace icu_tools {

  UNormalizationMode toNorm( const string& enc ){
    UNormalizationMode mode = UNORM_NFC;
    if ( enc == "NONE" )
      mode = UNORM_NONE;
    else if ( enc == "NFD" )
      mode = UNORM_NFD;
    else if ( enc == "NFC" )
      mode = UNORM_NFC;
    else if ( enc == "NFKC" )
      mode = UNORM_NFKC;
    else if ( enc == "NFKD" )
      mode = UNORM_NFKD;
    else
      throw std::logic_error( "invalid normalization mode: " + enc );
    return mode;
  }

  inline string toString( UNormalizationMode mode ){
    switch ( mode ){
    case UNORM_NONE:
      return "NONE";
    case UNORM_NFD:
      return "NFD";
    case UNORM_NFC:
      return "NFC";
    case UNORM_NFKC:
      return "NFKC";
    case UNORM_NFKD:
      return "NFKD";
    default:
      throw std::logic_error( "invalid normalization mode in switch" );
    }
  }

  std::string UnicodeNormalizer::getMode( ) const {
    return toString( mode );
  }

  std::string UnicodeNormalizer::setMode( const std::string& s ) {
    string res = getMode();
    mode = toNorm( s );
    return res;
  }

  UnicodeString UnicodeNormalizer::normalize( const UnicodeString& us ){
    UnicodeString r;
    UErrorCode status=U_ZERO_ERROR;
    Normalizer::normalize( us, mode, 0, r, status );
    if (U_FAILURE(status)){
      throw std::invalid_argument("Normalizer");
    }
    return r;
  }

  ostream& operator<<( ostream& os, const UnicodeFilter& q ){
    if ( q.empty() ){
      os << "none" << endl;
    }
    else {
      auto it=q.the_map.cbegin();
      while ( it != q.the_map.cend() ){
	os << folia::UnicodeToUTF8(UnicodeString(it->first)) << "\t" << it->second << endl;
	++it;
      }
    }
    return os;
  }

  UnicodeString UnicodeFilter::filter( const UnicodeString& s ){
    if ( empty() )
      return s;
    else {
      UnicodeString result;
      for ( int i=0; i < s.length(); ++i ){
	auto it=the_map.find(s[i]);
	if ( it != the_map.cend() )
	  result += it->second;
	else
	  result += s[i];
      }
      return result;
    }
  }

  bool UnicodeFilter::add( const string& s ){
    UnicodeString line = folia::UTF8ToUnicode(s);
    return add( line );
  }

  bool UnicodeFilter::add( const UnicodeString& s ){
    UnicodeString line = s;
    line.trim();
    if ((line.length() > 0) && (line[0] != '#')) {
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
	throw runtime_error( "invalid filter entry: "
			     + folia::UnicodeToUTF8(line) );
      }
      else {
	this->add( open[0], close );
      }
    }
    return true;
  }

  bool UnicodeFilter::fill( const string& s ){
    ifstream f ( s );
    if ( !f ){
      throw std::runtime_error("unable to open file: " + s );
    }
    else {
      string rawline;
      while ( getline(f,rawline) ){
	this->add( rawline );
      }
    }
    return true;
  }

  class uConfigError: public std::invalid_argument {
  public:
    uConfigError( const string& s ): invalid_argument( "ucto: config file:" + s ){};
    uConfigError( const UnicodeString& us ): invalid_argument( "ucto: config file:" + folia::UnicodeToUTF8(us) ){};
  };


  UnicodeString UnicodeRegexMatcher::Pattern() const{
    return pattern->pattern();
  }

  UnicodeRegexMatcher::UnicodeRegexMatcher( const UnicodeString& pat,
					    const UnicodeString& name ):
    _name(name)
  {
    failString.clear();
    matcher = NULL;
    UErrorCode u_stat = U_ZERO_ERROR;
    UParseError errorInfo;
    pattern = RegexPattern::compile( pat, 0, errorInfo, u_stat );
    if ( U_FAILURE(u_stat) ){
      string spat = folia::UnicodeToUTF8(pat);
      failString = folia::UnicodeToUTF8(_name);
      if ( errorInfo.offset >0 ){
	failString += " Invalid regular expression at position " + TiCC::toString( errorInfo.offset ) + "\n";
	UnicodeString pat1 = UnicodeString( pat, 0, errorInfo.offset -1 );
	failString += folia::UnicodeToUTF8(pat1) + " <== HERE\n";
      }
      else {
	failString += " Invalid regular expression '" + spat + "' ";
      }
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

} // namespace icu_tools
