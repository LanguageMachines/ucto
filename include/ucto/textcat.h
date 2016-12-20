/*
  Copyright (c) 2017
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

#ifndef TEXTCAT_H
#define TEXTCAT_H

#include <cstring>

#ifdef HAVE_TEXTCAT_H
#define ENABLE_TEXTCAT
#ifdef __cplusplus
extern "C" {
#endif

#include "textcat.h"

#ifdef __cplusplus
}
#endif

#else
#ifdef HAVE_LIBTEXTCAT_TEXTCAT_H
#include "libtextcat/textcat.h"
#define ENABLE_TEXTCAT
#else
#ifdef HAVE_LIBEXTTEXTCAT_TEXTCAT_H
#include "libexttextcat/textcat.h"
#define ENABLE_TEXTCAT
#endif
#endif
#endif

class TextCat {
 public:
  TextCat( const std::string& cf );
  TextCat( const TextCat& in );
  ~TextCat();
  bool isInit() const { return TC != 0; };
  std::string get_language( const std::string& ) const;
  std::vector<std::string> get_languages( const std::string& ) const;
 private:
  void *TC;
  std::string cfName;
};

#endif // TEXTCAT_H
