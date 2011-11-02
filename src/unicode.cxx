/*
  $Id$
  $URL$
  Copyright (c) 1998 - 2011
  ILK  -  Tilburg University
  CNTS -  University of Antwerp
 
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
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      Timbl@uvt.nl
*/

#include <string>
#include <ostream>
#include <map>
#include <stdexcept>
#include "libfolia/foliautils.h"
#include "ucto/unicode.h"

using namespace std;

namespace Tokenizer {

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
      map<UChar,UnicodeString>::const_iterator it=q.the_map.begin();
      while ( it != q.the_map.end() ){
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
	map<UChar,UnicodeString>::const_iterator it=the_map.find(s[i]);
	if ( it != the_map.end() )
	  result += it->second;
	else
	  result += s[i];
      }
      return result;
    }
  }

} // namespace Tokenizer
