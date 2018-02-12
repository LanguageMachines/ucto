/*
  Copyright (c) 2006 - 2018
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
#include "unicode/ustream.h"
#include "unicode/regex.h"
#include "unicode/ucnv.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
#include "ucto/unicode.h"

using namespace std;

namespace Tokenizer {

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

} // namespace Tokenizer
