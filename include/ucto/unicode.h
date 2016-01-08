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

#ifndef UCTO_UNICODE_H
#define UCTO_UNICODE_H

#include <string>
#include <map>
#include <iosfwd>
#include "unicode/ustream.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "unicode/normlzr.h"

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
    bool fill( const std::string& );
    bool add( const UnicodeString& );
    bool add( const std::string& );
    bool empty() const { return the_map.empty(); };
  private:
    void add( UChar uc, const UnicodeString& us ) { the_map[uc] = us; };
    std::map<UChar, UnicodeString> the_map;
  };

} // namespace

#endif // UCTO_UNICODE_H
