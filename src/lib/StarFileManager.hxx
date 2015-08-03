/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libstaroffice
* Version: MPL 2.0 / LGPLv2+
*
* The contents of this file are subject to the Mozilla Public License Version
* 2.0 (the "License"); you may not use this file except in compliance with
* the License or as specified alternatively below. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* Major Contributor(s):
* Copyright (C) 2002 William Lachance (wrlach@gmail.com)
* Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
* Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
* Copyright (C) 2006, 2007 Andrew Ziem
* Copyright (C) 2011, 2012 Alonso Laurent (alonso@loria.fr)
*
*
* All Rights Reserved.
*
* For minor contributions see the git repository.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

/*
 * StarFileManager to read/parse some basic StarOffice OLEs
 *
 */
#ifndef STAR_FILE_MANAGER
#  define STAR_FILE_MANAGER

#include <vector>

#include "STOFFDebug.hxx"
#include "STOFFEntry.hxx"
#include "STOFFInputStream.hxx"

namespace StarFileManagerInternal
{
struct State;
}

class StarZone;

/** \brief the main class to read/.. some basic StarOffice OLEs
 *
 *
 *
 */
class StarFileManager
{
public:
  //! constructor
  StarFileManager();
  //! destructor
  virtual ~StarFileManager();

  //! low level

  //!  the "persist elements" small ole: the list of object
  static bool readPersistElements(STOFFInputStreamPtr input, libstoff::DebugFile &ascii);
  //! try to read the document information : "SfxDocumentInformation"
  static bool readSfxDocumentInformation(STOFFInputStreamPtr input, libstoff::DebugFile &ascii);
  //! try to read the windows information : "SfxWindows"
  static bool readSfxWindows(STOFFInputStreamPtr input, libstoff::DebugFile &ascii);
  //! try to read the "Star Framework Config File"
  static bool readStarFrameworkConfigFile(STOFFInputStreamPtr input, libstoff::DebugFile &ascii);

  //! try tor read a image zone: "StarImageDocument" or "StarImageDocument 4.0
  static bool readImageDocument(STOFFInputStreamPtr input, librevenge::RVNGBinaryData &data, std::string const &fileName);
  //! try tor read a math zone: "StarMathDocument" (v 3 or v4)
  static bool readMathDocument(STOFFInputStreamPtr input, std::string const &fileName);

  //
  // data
  //
private:
  //! the state
  shared_ptr<StarFileManagerInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: