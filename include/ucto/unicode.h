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

#ifndef UNICODE_H
#define UNICODE_H

#include <string>
#include <map>
#include <iosfwd>
#include "unicode/ustream.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "unicode/normlzr.h"

// add two convenient fuctiond to the std:: namespace
UnicodeString UTF8ToUnicode( const std::string& );
std::string UnicodeToUTF8( const UnicodeString& );

namespace Tokenizer {

  class UnicodeNormalizer {
  public:
  UnicodeNormalizer(): mode(UNORM_NFC){};
    UnicodeString normalize( const UnicodeString& );
    std::string getMode( ) const;
    std::string setMode( const std::string& );
  private:
    UNormalizationMode mode;
  };
  
  class UnicodeFilter {
    friend std::ostream& operator<<( std::ostream&, const UnicodeFilter& );
  public:
    UnicodeString filter( const UnicodeString& );
    void add( UChar uc, const UnicodeString& us ) { the_map[uc] = us; };
    bool empty() const { return the_map.empty(); };
  private:
    std::map<UChar, UnicodeString> the_map;
  };
  
} // namespace

#endif // UNICODE_H
