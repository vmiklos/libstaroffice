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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "STOFFListener.hxx"
#include "STOFFOLEParser.hxx"

#include "StarAttribute.hxx"
#include "StarBitmap.hxx"
#include "StarGraphicStruct.hxx"
#include "StarObject.hxx"
#include "StarObjectSmallText.hxx"
#include "StarItemPool.hxx"
#include "StarZone.hxx"

#include "StarObjectSmallGraphic.hxx"

/** Internal: the structures of a StarObjectSmallGraphic */
namespace StarObjectSmallGraphicInternal
{
////////////////////////////////////////
//! Internal: virtual class to store a glue point
class GluePoint
{
public:
  //! constructor
  GluePoint(int x=0, int y=0) : m_dimension(x, y), m_direction(0), m_id(0), m_align(0), m_percent(false)
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, GluePoint const &pt)
  {
    o << "dim=" << pt.m_dimension << ",";
    if (pt.m_direction) o << "escDir=" << pt.m_direction << ",";
    if (pt.m_id) o << "id=" << pt.m_id << ",";
    if (pt.m_align) o << "align=" << pt.m_align << ",";
    if (pt.m_percent) o << "percent,";
    return o;
  }
  //! the dimension
  STOFFVec2i m_dimension;
  //! the esc direction
  int m_direction;
  //! the id
  int m_id;
  //! the alignment
  int m_align;
  //! a flag to know if this is percent
  bool m_percent;
};
////////////////////////////////////////
//! Internal: virtual class to store a outliner paragraph object
class OutlinerParaObject
{
public:
  //! small struct use to define a Zone: v<=3
  struct Zone {
    //! constructor
    Zone() : m_text(), m_depth(0), m_backgroundColor(STOFFColor::white()), m_background(), m_colorName("")
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Zone const &zone)
    {
      if (!zone.m_text) o << "noText,";
      if (zone.m_depth) o << "depth=" << zone.m_depth << ",";
      if (!zone.m_backgroundColor.isWhite()) o << "color=" << zone.m_backgroundColor << ",";
      if (!zone.m_background.isEmpty()) o << "hasBitmap,";
      if (!zone.m_colorName.empty()) o << "color[name]=" << zone.m_colorName.cstr() << ",";
      return o;
    }
    //! the text
    shared_ptr<StarObjectSmallText> m_text;
    //! the depth
    int m_depth;
    //! the background color
    STOFFColor m_backgroundColor;
    //! the background bitmap
    STOFFEmbeddedObject m_background;
    //! the color name
    librevenge::RVNGString m_colorName;
  };
  //! constructor
  OutlinerParaObject() : m_version(0), m_zones(), m_textZone(), m_depthList(), m_isEditDoc(false)
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, OutlinerParaObject const &obj)
  {
    o << "version=" << obj.m_version << ",";
    if (!obj.m_zones.empty()) {
      o << "zones=[";
      for (size_t i=0; i<obj.m_zones.size(); ++i)
        o << "[" << obj.m_zones[i] << "],";
      o << "],";
    }
    if (obj.m_textZone) o << "hasTextZone,";
    if (!obj.m_depthList.empty()) {
      o << "depth=[";
      for (size_t i=0; i<obj.m_depthList.size(); ++i)
        o << obj.m_depthList[i] << ",";
      o << "],";
    }
    if (obj.m_isEditDoc) o << "isEditDoc,";
    return o;
  }
  //! the version
  int m_version;
  //! the list of zones: version<=3
  std::vector<Zone> m_zones;
  //! list of text zone: version==4
  shared_ptr<StarObjectSmallText> m_textZone;
  //! list of depth data
  std::vector<int> m_depthList;
  //! true if the object is a edit document
  bool m_isEditDoc;
};

////////////////////////////////////////
//! Internal: virtual class to store a graphic
class Graphic
{
public:
  //! constructor
  Graphic(int id) : m_identifier(id)
  {
  }
  //! destructor
  virtual ~Graphic()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    return getName();
  }
  //! return the object name
  virtual std::string getName() const = 0;
  //! try to send the graphic to the listener
  bool send(STOFFListenerPtr /*listener*/)
  {
    static bool first=true;
    if (first) {
      first=false;
      STOFF_DEBUG_MSG(("StarObjectSmallGraphicInternal::Graphic::send: not implemented\n"));
    }
    return false;
  }

  //! the type
  int m_identifier;
};

////////////////////////////////////////
//! Internal: virtual class to store a SCHU graphic
class SCHUGraphic : public Graphic
{
public:
  //! constructor
  SCHUGraphic(int id) : Graphic(id), m_id(0), m_adjust(0), m_orientation(0), m_column(0), m_row(0), m_factor(0)
  {
  }
  //! return the object name
  std::string getName() const
  {
    if (m_identifier>0 && m_identifier<=7) {
      char const *(wh[])= {"none", "group",  "objectId", "objectAdjustId", "dataRowId",
                           "dataPointId", "lightfactorId", "axisId"
                          };
      return wh[m_identifier];
    }
    std::stringstream s;
    s << "###type=" << m_identifier << "[SCHU],";
    return s.str();
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SCHUGraphic const &graph)
  {
    o << graph.getName() << ",";
    switch (graph.m_identifier) {
    case 2:
    case 7:
      o << "id=" << graph.m_id << ",";
      break;
    case 3:
      o << "adjust=" << graph.m_adjust << ",";
      if (graph.m_orientation)
        o << "orientation=" << graph.m_orientation << ",";
      break;
    case 4:
      o << "row=" << graph.m_row << ",";
      break;
    case 5:
      o << "column=" << graph.m_column << ",";
      o << "row=" << graph.m_row << ",";
      break;
    case 6:
      o << "factor=" << graph.m_factor << ",";
      break;
    default:
      break;
    }
    return o;
  }
  //! the id
  int m_id;
  //! the adjust data
  int m_adjust;
  //! the orientation
  int m_orientation;
  //! the column
  int m_column;
  //! the row
  int m_row;
  //! the factor
  double m_factor;
};

////////////////////////////////////////
//! Internal: virtual class to store a SDUD graphic
class SDUDGraphic : public Graphic
{
public:
  //! constructor
  SDUDGraphic(int id) : Graphic(id)
  {
  }
  //! return the object name
  std::string getName() const
  {
    if (m_identifier>0 && m_identifier<=2) {
      char const *(wh[])= {"none", "animationInfo",  "imapInfo" };
      return wh[m_identifier];
    }
    std::stringstream s;
    s << "###type=" << m_identifier << "[SDUD],";
    return s.str();
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SDUDGraphic const &graph)
  {
    o << graph.getName() << ",";
    return o;
  }
};

////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic
class SdrGraphic : public Graphic
{
public:
  //! constructor
  SdrGraphic(int id) : Graphic(id), m_bdbox(), m_layerId(-1), m_anchorPosition(0,0), m_polygon()
  {
    for (int i=0; i<6; ++i) m_flags[i]=false;
  }
  //! return the object name
  std::string getName() const
  {
    if (m_identifier>0 && m_identifier<=32) {
      char const *(wh[])= {"none", "group", "line", "rect", "circle",
                           "sector", "arc", "ccut", "poly", "polyline",
                           "pathline", "pathfill", "freeline", "freefill", "splineline",
                           "splinefill", "text", "textextended", "fittext", "fitalltext",
                           "titletext", "outlinetext", "graf", "ole2", "edge",
                           "caption", "pathpoly", "pathline", "page", "measure",
                           "dummy","frame", "uno"
                          };
      return wh[m_identifier];
    }
    std::stringstream s;
    s << "###type=" << m_identifier << ",";
    return s.str();
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphic const &graph)
  {
    o << graph.getName() << ",";
    o << "bdbox=" << graph.m_bdbox << ",";
    if (graph.m_layerId>=0) o << "layer[id]=" << graph.m_layerId << ",";
    if (graph.m_anchorPosition!=STOFFVec2i(0,0)) o << "anchor[pos]=" << graph.m_anchorPosition << ",";
    for (int i=0; i<6; ++i) {
      if (!graph.m_flags[i]) continue;
      char const *(wh[])= {"move[protected]", "size[protected]", "print[no]", "mark[protected]", "empty", "notVisibleAsMaster"};
      o << wh[i] << ",";
    }
    if (!graph.m_polygon.empty()) {
      o << "poly=[";
      for (size_t i=0; i<graph.m_polygon.size(); ++i)
        o << graph.m_polygon[i] << ",";
      o << "],";
    }
    return o;
  }
  //! the bdbox
  STOFFBox2i m_bdbox;
  //! the layer id
  int m_layerId;
  //! the anchor position
  STOFFVec2i m_anchorPosition;
  //! a polygon
  std::vector<GluePoint> m_polygon;
  //! a list of flag
  bool m_flags[6];
  //TODO: store the user data
};

////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic attribute
class SdrGraphicAttribute : public SdrGraphic
{
public:
  //! constructor
  SdrGraphicAttribute(int id) : SdrGraphic(id), m_itemList(), m_sheetStyle("")
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphic::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicAttribute const &graph)
  {
    o << graph.getName() << ",";
    for (size_t i=0; i<graph.m_itemList.size(); ++i) {
      if (!graph.m_itemList[i] || !graph.m_itemList[i]->m_attribute) continue;
      libstoff::DebugStream f;
      graph.m_itemList[i]->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    if (!graph.m_sheetStyle.empty()) o << "sheetStyle[name]=" << graph.m_sheetStyle.cstr() << ",";
    return o;
  }
  //! the list of star item
  std::vector<shared_ptr<StarItem> > m_itemList;
  //! the sheet style name
  librevenge::RVNGString m_sheetStyle;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic group
class SdrGraphicGroup : public SdrGraphic
{
public:
  //! constructor
  SdrGraphicGroup(int id) : SdrGraphic(id), m_groupName(), m_child(), m_refPoint(), m_hasRefPoint(false), m_groupDrehWink(0), m_groupShearWink(0)
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphic::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicGroup const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_groupName.empty()) o << graph.m_groupName.cstr() << ",";
    if (!graph.m_child.empty()) o << "num[child]=" << graph.m_child.size() << ",";
    if (graph.m_hasRefPoint) o << "refPt=" << graph.m_refPoint << ",";
    if (graph.m_groupDrehWink) o << "drehWink=" << graph.m_groupDrehWink << ",";
    if (graph.m_groupShearWink) o << "shearWink=" << graph.m_groupShearWink << ",";
    return o;
  }
  //! the group name
  librevenge::RVNGString m_groupName;
  //! the child
  std::vector<shared_ptr<StarObjectSmallGraphic> > m_child;
  //! the ref point
  STOFFVec2i m_refPoint;
  //! flag to know if we use the ref point
  bool m_hasRefPoint;
  //! the dreh wink: rotation?
  int m_groupDrehWink;
  //! the shear wink
  int m_groupShearWink;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic text
class SdrGraphicText : public SdrGraphicAttribute
{
public:
  //! constructor
  SdrGraphicText(int id) : SdrGraphicAttribute(id), m_textKind(0), m_textRectangle(),
    m_textDrehWink(0), m_textShearWink(0), m_outlinerParaObject(), m_textBound()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicAttribute::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicText const &graph)
  {
    o << graph.getName() << ",";
    o << "textKind=" << graph.m_textKind << ",";
    o << "rect=" << graph.m_textRectangle << ",";
    if (graph.m_textDrehWink) o << "drehWink=" << graph.m_textDrehWink << ",";
    if (graph.m_textShearWink) o << "shearWink=" << graph.m_textShearWink << ",";
    if (graph.m_outlinerParaObject) o << "outliner=[" << *graph.m_outlinerParaObject << "],";
    if (graph.m_textBound.size()!=STOFFVec2i(0,0)) o << "bound=" << graph.m_textBound << ",";
    return o;
  }
  //! the text kind
  int m_textKind;
  //! the text rectangle
  STOFFBox2i m_textRectangle;
  //! the dreh wink: rotation?
  int m_textDrehWink;
  //! the shear wink
  int m_textShearWink;
  //! the outliner object
  shared_ptr<OutlinerParaObject> m_outlinerParaObject;
  //! the text bound
  STOFFBox2i m_textBound;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic rectangle
class SdrGraphicRect : public SdrGraphicText
{
public:
  //! constructor
  SdrGraphicRect(int id) : SdrGraphicText(id), m_eckRag(0)
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicText::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicRect const &graph)
  {
    o << graph.getName() << ",";
    if (graph.m_eckRag) o << "eckRag=" << graph.m_eckRag << ",";
    return o;
  }
  //! the eckRag?
  int m_eckRag;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic caption
class SdrGraphicCaption : public SdrGraphicRect
{
public:
  //! constructor
  SdrGraphicCaption() : SdrGraphicRect(25), m_captionPolygon(), m_captionItem()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicRect::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicCaption const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_captionPolygon.empty()) {
      o << "poly=[";
      for (size_t i=0; i<graph.m_captionPolygon.size(); ++i)
        o << graph.m_captionPolygon[i] << ",";
      o << "],";
    }
    if (graph.m_captionItem && graph.m_captionItem->m_attribute) {
      libstoff::DebugStream f;
      graph.m_captionItem->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    return o;
  }
  //! a polygon
  std::vector<STOFFVec2i> m_captionPolygon;
  //! the caption attributes
  shared_ptr<StarItem> m_captionItem;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic circle
class SdrGraphicCircle : public SdrGraphicRect
{
public:
  //! constructor
  SdrGraphicCircle(int id) : SdrGraphicRect(id), m_circleItem()
  {
    m_angles[0]=m_angles[1]=0;
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicRect::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicCircle const &graph)
  {
    o << graph.getName() << ",";
    if (graph.m_angles[0]<0||graph.m_angles[0]>0 || graph.m_angles[1]<0||graph.m_angles[1]>0)
      o << "angles=" << graph.m_angles[0] << "x" << graph.m_angles[1] << ",";
    if (graph.m_circleItem && graph.m_circleItem->m_attribute) {
      libstoff::DebugStream f;
      graph.m_circleItem->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    return o;
  }
  //! the two angles
  float m_angles[2];
  //! the circle attributes
  shared_ptr<StarItem> m_circleItem;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic edge
class SdrGraphicEdge : public SdrGraphicText
{
public:
  //! the information record
  struct Information {
    //! constructor
    Information() : m_orthoForm(0)
    {
      m_angles[0]=m_angles[1]=0;
      for (int i=0; i<3; ++i) m_n[i]=0;
    }
    //! operator=
    friend std::ostream &operator<<(std::ostream &o, Information const &info)
    {
      o << "pts=[";
      for (int i=0; i<5; ++i) o << info.m_points[i] << ",";
      o << "],";
      o << "angles=" << info.m_angles[0] << "x" << info.m_angles[1] << ",";
      for (int i=0; i<3; ++i) {
        if (info.m_n[i]) o << "n" << i << "=" << info.m_n[i] << ",";
      }
      if (info.m_orthoForm) o << "orthoForm=" << info.m_orthoForm << ",";
      return o;
    }
    //! some points: obj1Line2, obj1Line3, obj2Line2, obj2Line3, middleLine
    STOFFVec2i m_points[5];
    //! two angles
    int m_angles[2];
    //! some values: nObj1Lines, nObj2Lines, middleLines
    int m_n[3];
    //! orthogonal form
    int m_orthoForm;
  };
  //! constructor
  SdrGraphicEdge() : SdrGraphicText(24), m_edgePolygon(), m_edgePolygonFlags(), m_edgeItem(), m_info()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicText::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicEdge const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_edgePolygon.empty()) {
      if (graph.m_edgePolygon.size()==graph.m_edgePolygonFlags.size()) {
        o << "poly=[";
        for (size_t i=0; i<graph.m_edgePolygon.size(); ++i)
          o << graph.m_edgePolygon[i] << ":" << graph.m_edgePolygonFlags[i] << ",";
        o << "],";
      }
      else {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphicInternal::SdrGraphicEdge::operator<<: unexpected number of flags\n"));
        o << "###poly,";
      }
    }
    if (graph.m_edgeItem && graph.m_edgeItem->m_attribute) {
      libstoff::DebugStream f;
      graph.m_edgeItem->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    return o;
  }
  //! the edge polygon
  std::vector<STOFFVec2i> m_edgePolygon;
  //! the edge polygon flags
  std::vector<int> m_edgePolygonFlags;
  // TODO: store the connector
  //! the edge attributes
  shared_ptr<StarItem> m_edgeItem;
  //! the information record
  Information m_info;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic graph
class SdrGraphicGraph : public SdrGraphicRect
{
public:
  //! constructor
  SdrGraphicGraph() : SdrGraphicRect(22), m_bitmap(), m_graphRectangle(), m_mirrored(false), m_hasGraphicLink(false), m_graphItem()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicRect::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicGraph const &graph)
  {
    o << graph.getName() << ",";
    if (graph.m_bitmap) o << "hasBitmap,";
    if (graph.m_graphRectangle.size()[0] || graph.m_graphRectangle.size()[1]) o << "rect=" << graph.m_graphRectangle << ",";
    for (int i=0; i<3; ++i) {
      if (graph.m_graphNames[i].empty()) continue;
      o << (i==0 ? "name" : i==1 ? "file[name]" : "filter[name]") << "=" << graph.m_graphNames[i].cstr() << ",";
    }
    if (graph.m_mirrored) o << "mirrored,";
    if (graph.m_hasGraphicLink) o << "hasGraphicLink,";
    if (graph.m_graphItem && graph.m_graphItem->m_attribute) {
      libstoff::DebugStream f;
      graph.m_graphItem->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    return o;
  }
  //! the bitmap
  shared_ptr<StarBitmap> m_bitmap;
  //! the rectangle
  STOFFBox2i m_graphRectangle;
  //! the name, filename, the filtername
  librevenge::RVNGString m_graphNames[3];
  //! flag to know if the image is mirrored
  bool m_mirrored;
  //! flag to know if the image has a graphic link
  bool m_hasGraphicLink;
  //! the graph attributes
  shared_ptr<StarItem> m_graphItem;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic edge
class SdrGraphicMeasure : public SdrGraphicText
{
public:
  //! constructor
  SdrGraphicMeasure() : SdrGraphicText(29), m_overwritten(false), m_measureItem()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicText::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicMeasure const &graph)
  {
    o << graph.getName() << ",";
    if (graph.m_overwritten) o << "overwritten,";
    o << "pts=[";
    for (int i=0; i<2; ++i) o << graph.m_measurePoints[i] << ",";
    o << "],";
    if (graph.m_measureItem && graph.m_measureItem->m_attribute) {
      libstoff::DebugStream f;
      graph.m_measureItem->m_attribute->print(f);
      o << "[" << f.str() << "],";
    }
    return o;
  }
  //! the points
  STOFFVec2i m_measurePoints[2];
  //! overwritten flag
  bool m_overwritten;
  //! the measure attributes
  shared_ptr<StarItem> m_measureItem;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic OLE
class SdrGraphicOLE : public SdrGraphicRect
{
public:
  //! constructor
  SdrGraphicOLE(int id) : SdrGraphicRect(id), m_bitmap()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicRect::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicOLE const &graph)
  {
    o << graph.getName() << ",";
    for (int i=0; i<2; ++i) {
      if (!graph.m_oleNames[i].empty())
        o << (i==0 ? "persist" : "program") << "[name]=" << graph.m_oleNames[i].cstr() << ",";
    }
    if (graph.m_bitmap) o << "hasBitmap,";
    return o;
  }
  //! the persist and the program name
  librevenge::RVNGString m_oleNames[2];
  //! the bitmap
  shared_ptr<StarBitmap> m_bitmap;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic page
class SdrGraphicPage : public SdrGraphic
{
public:
  //! constructor
  SdrGraphicPage() : SdrGraphic(28), m_page(0)
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphic::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicPage const &graph)
  {
    if (graph.m_page>=0) o << "page=" << graph.m_page << ",";
    return o;
  }
  //! the page
  int m_page;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic path
class SdrGraphicPath : public SdrGraphicText
{
public:
  //! constructor
  SdrGraphicPath(int id) : SdrGraphicText(id), m_pathPolygon(), m_pathPolygonFlags()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicText::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicPath const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_pathPolygon.empty()) {
      if (graph.m_pathPolygonFlags.empty()) {
        o << "poly=[";
        for (size_t i=0; i<graph.m_pathPolygon.size(); ++i)
          o << graph.m_pathPolygon[i] << ",";
        o << "],";
      }
      else if (graph.m_pathPolygon.size()==graph.m_pathPolygonFlags.size()) {
        o << "poly=[";
        for (size_t i=0; i<graph.m_pathPolygon.size(); ++i)
          o << graph.m_pathPolygon[i] << ":" << graph.m_pathPolygonFlags[i] << ",";
        o << "],";
      }
      else {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphicInternal::SdrGraphicPath::operator<<: unexpected number of flags\n"));
        o << "###poly,";
      }
    }
    return o;
  }
  //! the path polygon
  std::vector<STOFFVec2i> m_pathPolygon;
  //! the path polygon flags
  std::vector<int> m_pathPolygonFlags;
};
////////////////////////////////////////
//! Internal: virtual class to store a Sdr graphic uno
class SdrGraphicUno : public SdrGraphicRect
{
public:
  //! constructor
  SdrGraphicUno() : SdrGraphicRect(32), m_unoName()
  {
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << SdrGraphicRect::print() << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SdrGraphicUno const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_unoName.empty()) o << graph.m_unoName.cstr() << ",";
    return o;
  }
  //! the uno name
  librevenge::RVNGString m_unoName;
};
////////////////////////////////////////
//! Internal: virtual class to store a SDUD graphic animation
class SDUDGraphicAnimation : public SDUDGraphic
{
public:
  //! constructor
  SDUDGraphicAnimation() : SDUDGraphic(1), m_polygon(), m_order(0)
  {
    for (int i=0; i<8; ++i) m_values[i]=0;
    for (int i=0; i<2; ++i) m_colors[i]=STOFFColor::white();
    for (int i=0; i<3; ++i) m_flags[i]=false;
    for (int i=0; i<5; ++i) m_booleans[i]=false;
  }
  //! basic print function
  virtual std::string print() const
  {
    std::stringstream s;
    s << *this << ",";
    return s.str();
  }
  //! print object data
  friend std::ostream &operator<<(std::ostream &o, SDUDGraphicAnimation const &graph)
  {
    o << graph.getName() << ",";
    if (!graph.m_polygon.empty()) {
      o << "poly=[";
      for (size_t i=0; i<graph.m_polygon.size(); ++i)
        o << graph.m_polygon[i] << ",";
      o << "],";
    }
    if (graph.m_limits[0]!=STOFFVec2i(0,0)) o << "start=" << graph.m_limits[0] << ",";
    if (graph.m_limits[1]!=STOFFVec2i(0,0)) o << "end=" << graph.m_limits[1] << ",";
    for (int i=0; i<8; ++i) {
      if (!graph.m_values[i]) continue;
      char const *(wh[])= {"pres[effect]", "speed", "clickAction", "pres[effect,second]", "speed[second]",
                           "invisible", "verb", "text[effect]"
                          };
      o << wh[i] << "=" << graph.m_values[i] << ",";
    }
    for (int i=0; i<3; ++i) {
      if (!graph.m_flags[i]) continue;
      char const *(wh[])= {"active", "dim[previous]", "isMovie"};
      o << wh[i] << ",";
    }
    for (int i=0; i<2; ++i) {
      if (!graph.m_colors[i].isWhite())
        o << (i==0 ? "blueScreen" : "dim[color]") << "=" << graph.m_colors[i] << ",";
    }
    for (int i=0; i<3; ++i) {
      if (graph.m_names[i].empty()) continue;
      char const *(wh[])= {"sound[file]", "bookmark", "sound[file,second]"};
      o << wh[i] << "=" << graph.m_names[i].cstr() << ",";
    }
    for (int i=0; i<5; ++i) {
      if (!graph.m_booleans[i]) continue;
      char const *(wh[])= {"hasSound", "playFull","hasSound[second]", "playFull[second]","dim[hide]"};
      o << wh[i] << ",";
    }
    if (graph.m_order) o << "order=" << graph.m_order << ",";
    return o;
  }
  //! the polygon
  std::vector<STOFFVec2i> m_polygon;
  //! the limits start, end
  STOFFVec2i m_limits[2];
  //! the values: presentation effect, speed, clickAction, presentation effect[second], speed[second], invisible, verb, text effect
  int m_values[8];
  //! the colors
  STOFFColor m_colors[2];
  //! some flags : active, dim[previous], isMovie
  bool m_flags[3];
  //! some bool : hasSound, playFull, sound[second], playFull[second], dim[hide]
  bool m_booleans[5];
  //! the names : sound file, bookmark, sound file[second]
  librevenge::RVNGString m_names[3];
  //! the presentation order
  int m_order;
  // TODO add surrogate
};

////////////////////////////////////////
//! Internal: the state of a StarObjectSmallGraphic
struct State {
  //! constructor
  State() : m_graphic()
  {
  }
  //! the graphic object
  shared_ptr<Graphic> m_graphic;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
StarObjectSmallGraphic::StarObjectSmallGraphic(StarObject const &orig, bool duplicateState) : StarObject(orig, duplicateState), m_graphicState(new StarObjectSmallGraphicInternal::State)
{
}

StarObjectSmallGraphic::~StarObjectSmallGraphic()
{
}

std::ostream &operator<<(std::ostream &o, StarObjectSmallGraphic const &graphic)
{
  if (graphic.m_graphicState->m_graphic)
    o << graphic.m_graphicState->m_graphic->print();
  return o;
}

bool StarObjectSmallGraphic::send(STOFFListenerPtr listener)
{
  if (!listener) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::send: can not find the listener\n"));
    return false;
  }
  if (!m_graphicState->m_graphic) {
    static bool first=true;
    if (first) {
      first=false;
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::send: no object\n"));
    }
    return false;
  }
  return m_graphicState->m_graphic->send(listener);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////

bool StarObjectSmallGraphic::readSdrObject(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  // first check magic
  std::string magic("");
  long pos=input->tell();
  for (int i=0; i<4; ++i) magic+=(char) input->readULong(1);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (magic!="DrOb" || !zone.openSDRHeader(magic)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrObject)[" << zone.getRecordLevel() << "]:";
  int version=zone.getHeaderVersion();
  f << magic << ",nVers=" << version << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long lastPos=zone.getRecordLastPosition();
  if (lastPos==input->tell()) {
    zone.closeSDRHeader("SdrObject");
    return true;
  }
  // svdobj.cxx SdrObjFactory::MakeNewObject
  pos=input->tell();
  f.str("");
  f << "SdrObject:";
  magic="";
  for (int i=0; i<4; ++i) magic+=(char) input->readULong(1);
  uint16_t identifier;
  *input>>identifier;
  f << magic << ", ident=" << std::hex << identifier << std::dec << ",";
  bool ok=true;
  shared_ptr<StarObjectSmallGraphicInternal::Graphic> graphic;
  if (magic=="SVDr")
    graphic=readSVDRObject(zone, (int) identifier);
  else if (magic=="SCHU")
    graphic=readSCHUObject(zone, (int) identifier);
  else if (magic=="FM01") // FmFormInventor
    graphic=readFmFormObject(zone, (int) identifier);
  // to do magic=="E3D1" // E3dInventor
  if (graphic)
    m_graphicState->m_graphic=graphic;
  else {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObject: can not read an object\n"));
    f << "###";
    ok=false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (ok) {
    pos=input->tell();
    if (pos==lastPos) {
      zone.closeSDRHeader("SdrObject");
      return true;
    }
    f.str("");
    f << "SVDR:##extra";
    static bool first=true;
    if (first) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObject: read object, find extra data\n"));
      first=false;
    }
    f << "##";
  }
  if (pos!=input->tell())
    ascFile.addDelimiter(input->tell(),'|');

  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeSDRHeader("SdrObject");
  return true;
}

////////////////////////////////////////////////////////////
//  SVDR
////////////////////////////////////////////////////////////
shared_ptr<StarObjectSmallGraphicInternal::SdrGraphic> StarObjectSmallGraphic::readSVDRObject(StarZone &zone, int identifier)
{
  STOFFInputStreamPtr input=zone.input();
  long pos;

  long endPos=zone.getRecordLastPosition();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;

  bool ok=true;
  char const *(wh[])= {"none", "group", "line", "rect", "circle",
                       "sector", "arc", "ccut", "poly", "polyline",
                       "pathline", "pathfill", "freeline", "freefill", "splineline",
                       "splinefill", "text", "textextended", "fittext", "fitalltext",
                       "titletext", "outlinetext", "graf", "ole2", "edge",
                       "caption", "pathpoly", "pathline", "page", "measure",
                       "dummy","frame", "uno"
                      };
  shared_ptr<StarObjectSmallGraphicInternal::SdrGraphic> graphic;
  switch (identifier) {
  case 1: { // group
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicGroup> graphicGroup(new StarObjectSmallGraphicInternal::SdrGraphicGroup(identifier));
    graphic=graphicGroup;
    ok=readSVDRObjectGroup(zone, *graphicGroup);
    break;
  }
  case 2: // line
  case 8: // poly
  case 9: // polyline
  case 10: // pathline
  case 11: // pathfill
  case 12: // freeline
  case 13: // freefill
  case 26: // pathpoly
  case 27: { // pathline
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicPath> graphicPath(new StarObjectSmallGraphicInternal::SdrGraphicPath(identifier));
    graphic=graphicPath;
    ok=readSVDRObjectPath(zone, *graphicPath);
    break;
  }
  case 4: // circle
  case 5: // sector
  case 6: // arc
  case 7: { // cut
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicCircle> graphicCircle(new StarObjectSmallGraphicInternal::SdrGraphicCircle(identifier));
    graphic=graphicCircle;
    ok=readSVDRObjectCircle(zone, *graphicCircle);
    break;
  }
  case 3: // rect
  case 16: // text
  case 17: // textextended
  case 20: // title text
  case 21: { // outline text
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicRect> graphicRect(new StarObjectSmallGraphicInternal::SdrGraphicRect(identifier));
    graphic=graphicRect;
    ok=readSVDRObjectRect(zone, *graphicRect);
    break;
  }
  case 24: { // edge
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicEdge> graphicEdge(new StarObjectSmallGraphicInternal::SdrGraphicEdge());
    graphic=graphicEdge;
    ok=readSVDRObjectEdge(zone, *graphicEdge);
    break;
  }
  case 22: { // graph
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicGraph> graphicGraph(new StarObjectSmallGraphicInternal::SdrGraphicGraph());
    graphic=graphicGraph;
    ok=readSVDRObjectGraph(zone, *graphicGraph);
    break;
  }
  case 23: // ole
  case 31: { // frame
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicOLE> graphicOLE(new StarObjectSmallGraphicInternal::SdrGraphicOLE(identifier));
    graphic=graphicOLE;
    ok=readSVDRObjectOLE(zone, *graphicOLE);
    break;
  }
  case 25: { // caption
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicCaption> graphicCaption(new StarObjectSmallGraphicInternal::SdrGraphicCaption());
    graphic=graphicCaption;
    ok=readSVDRObjectCaption(zone, *graphicCaption);
    break;
  }
  case 28: { // page
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicPage> graphicPage(new StarObjectSmallGraphicInternal::SdrGraphicPage());
    graphic=graphicPage;
    ok=readSVDRObjectHeader(zone, *graphicPage);
    if (!ok) break;
    pos=input->tell();
    if (!zone.openRecord()) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: can not open page record\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=false;
      break;
    }
    graphicPage->m_page=(int) input->readULong(2);
    f << "SVDR[page]:page=" << graphicPage->m_page << ",";
    ok=input->tell()<=zone.getRecordLastPosition();
    if (!ok)
      f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeRecord("SVDR");
    break;
  }
  case 29: { // measure
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicMeasure> graphicMeasure(new StarObjectSmallGraphicInternal::SdrGraphicMeasure());
    graphic=graphicMeasure;
    ok=readSVDRObjectMeasure(zone, *graphicMeasure);
    break;
  }
  case 32: { // uno
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicUno> graphicUno(new StarObjectSmallGraphicInternal::SdrGraphicUno());
    graphic=graphicUno;
    ok=readSVDRObjectRect(zone, *graphicUno);
    pos=input->tell();
    if (!zone.openRecord()) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: can not open uno record\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=false;
      break;
    }
    f << "SVDR[uno]:";
    // + SdrUnoObj::ReadData (checkme)
    std::vector<uint32_t> string;
    if (input->tell()!=zone.getRecordLastPosition() && (!zone.readString(string) || input->tell()>zone.getRecordLastPosition())) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: can not read uno string\n"));
      f << "###uno";
      ok=false;
    }
    else if (!string.empty()) {
      graphicUno->m_unoName=libstoff::getString(string);
      f << graphicUno->m_unoName.cstr() << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeRecord("SVDR");
    break;
  }
  default:
    graphic.reset(new StarObjectSmallGraphicInternal::SdrGraphic(identifier));
    ok=readSVDRObjectHeader(zone, *graphic);
    break;
  }
  pos=input->tell();
  if (!ok) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: can not read some zone\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(SVDR):###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    graphic.reset(new StarObjectSmallGraphicInternal::SdrGraphic(identifier));
    return graphic;
  }
  if (input->tell()==endPos)
    return graphic;
  graphic.reset(new StarObjectSmallGraphicInternal::SdrGraphic(identifier));
  static bool first=true;
  if (first) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: find unexpected data\n"));
  }
  if (identifier<=0 || identifier>32) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObject: unknown identifier\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(SVDR):###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return graphic;
  }

  while (input->tell()<endPos) {
    pos=input->tell();
    f.str("");
    f << "SVDR:" << wh[identifier] << ",###unknown,";
    if (!zone.openRecord())
      return graphic;
    long lastPos=zone.getRecordLastPosition();
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
    zone.closeRecord("SVDR");
  }
  return graphic;
}

bool StarObjectSmallGraphic::readSVDRObjectAttrib(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicAttribute &graphic)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  if (!readSVDRObjectHeader(zone, graphic)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;

  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectAttrib: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  long lastPos=zone.getRecordLastPosition();
  shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
  if (!pool)
    pool=getNewItemPool(StarItemPool::T_VCControlPool);
  int vers=zone.getHeaderVersion();
  // svx_svdoattr: SdrAttrObj:ReadData
  bool ok=true;
  f << "[";
  for (int i=0; i<6; ++i) {
    if (vers<11) input->seek(2, librevenge::RVNG_SEEK_CUR);
    uint16_t const(what[])= {1017/*XATTRSET_LINE*/, 1047/*XATTRSET_FILL*/, 1066/*XATTRSET_TEXT*/, 1079/*SDRATTRSET_SHADOW*/,
                             1096 /*SDRATTRSET_OUTLINER*/, 1126 /*SDRATTRSET_MISC*/
                            };
    uint16_t nWhich=what[i];
    shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
    if (!item || input->tell()>lastPos) {
      f << "###";
      ok=false;
      break;
    }
    graphic.m_itemList.push_back(item);
    if (vers<5 && i==3) break;
    if (vers<6 && i==4) break;
  }
  f << "],";
  std::vector<uint32_t> string;
  if (ok && (!zone.readString(string) || input->tell()>lastPos)) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectAttrib: can not read the sheet style name\n"));
    ok=false;
  }
  else if (!string.empty()) {
    graphic.m_sheetStyle=libstoff::getString(string);
    f << "eFamily=" << input->readULong(2) << ",";
    if (vers>0 && vers<11) // in this case, we must convert the style name
      f << "charSet=" << input->readULong(2) << ",";
  }
  if (ok && vers==9 && input->tell()+2==lastPos) // probably a charset even when string.empty()
    f << "#charSet?=" << input->readULong(2) << ",";
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectAttrib: find extra data\n"));
      f << "###extra,vers=" << vers;
    }
    ascFile.addDelimiter(input->tell(),'|');
  }
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");

  std::string extra=f.str();
  f.str("");
  f << "SVDR[" << zone.getRecordLevel() << "]:attrib," << graphic << extra;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectCaption(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicCaption &graphic)
{
  if (!readSVDRObjectRect(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:caption,";
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectCaption: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  // svx_svdocapt.cxx SdrCaptionObj::ReadData
  bool ok=true;
  uint16_t n;
  *input >> n;
  if (input->tell()+8*n>lastPos) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectCaption: the number of point seems bad\n"));
    f << "###n=" << n << ",";
    ok=false;
    n=0;
  }
  for (int pt=0; pt<int(n); ++pt) {
    int dim[2];
    for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
    graphic.m_captionPolygon.push_back(STOFFVec2i(dim[0],dim[1]));
  }
  if (ok) {
    shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
    if (!pool)
      pool=getNewItemPool(StarItemPool::T_XOutdevPool);
    uint16_t nWhich=1195; // SDRATTRSET_CAPTION
    shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
    if (!item || input->tell()>lastPos)
      f << "###";
    else
      graphic.m_captionItem=item;
  }
  f << graphic;
  if (!ok) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeRecord("SVDR");

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectCircle(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicCircle &graphic)
{
  if (!readSVDRObjectRect(zone, graphic))
    return false;
  int const &id=graphic.m_identifier;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  // svx_svdocirc SdrCircObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectCircle: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  if (id!=4) {
    for (int i=0; i<2; ++i)
      graphic.m_angles[i]=float(input->readLong(4))/100.f;
  }
  if (input->tell()!=lastPos) {
    shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
    if (!pool)
      pool=getNewItemPool(StarItemPool::T_XOutdevPool);
    uint16_t nWhich=1179; // SDRATTRSET_CIRC
    shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
    if (!item || input->tell()>lastPos) {
      f << "###";
    }
    else
      graphic.m_circleItem=item;
  }
  zone.closeRecord("SVDR");

  std::string extra=f.str();
  f.str("");
  f << "SVDR[" << zone.getRecordLevel() << "]:" << graphic << extra;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectEdge(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicEdge &graphic)
{
  if (!readSVDRObjectText(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:";
  // svx_svdoedge SdrEdgeObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  int vers=zone.getHeaderVersion();
  bool ok=true;
  if (vers<2) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: unexpected version\n"));
    f << "##badVers,";
    ok=false;
  }

  bool openRec=false;
  if (ok && vers>=11) {
    openRec=zone.openRecord();
    if (!openRec) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: can not edgeTrack record\n"));
      f << "###record";
      ok=false;
    }
  }
  if (ok) {
    uint16_t n;
    *input >> n;
    if (input->tell()+9*n>zone.getRecordLastPosition()) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: the number of point seems bad\n"));
      f << "###n=" << n << ",";
      ok=false;
    }
    else {
      for (int pt=0; pt<int(n); ++pt) {
        int dim[2];
        for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
        graphic.m_edgePolygon.push_back(STOFFVec2i(dim[0],dim[1]));
      }
      for (int pt=0; pt<int(n); ++pt) graphic.m_edgePolygonFlags.push_back((int)input->readULong(1));
    }
  }
  f << graphic;
  if (openRec) {
    if (!ok) input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
    zone.closeRecord("SVDR");
  }
  if (ok && input->tell()<lastPos) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    f.str("");
    f << "SVDR[edgeB]:";
    pos=input->tell();

    for (int i=0; i<2; ++i) { // TODO: storeme
      if (!readSDRObjectConnection(zone)) {
        f << "##connector,";
        ok=false;
        break;
      }
      pos=input->tell();
    }
  }
  if (ok && input->tell()<lastPos) {
    shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
    if (!pool)
      pool=getNewItemPool(StarItemPool::T_XOutdevPool);
    uint16_t nWhich=1146; // SDRATTRSET_EDGE
    shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
    if (!item || input->tell()>lastPos) {
      f << "###";
    }
    else {
      graphic.m_edgeItem=item;
      if (item->m_attribute)
        item->m_attribute->print(f);
    }
  }
  if (ok && input->tell()<lastPos) {
    // svx_svdoedge.cxx SdrEdgeInfoRec operator>>
    if (input->tell()+5*8+2*4+3*2+1>lastPos) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: SdrEdgeInfoRec seems too short\n"));
      ok=false;
    }
    else {
      StarObjectSmallGraphicInternal::SdrGraphicEdge::Information &info=graphic.m_info;
      for (int pt=0; pt<5; ++pt) {
        int dim[2];
        for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
        info.m_points[pt]=STOFFVec2i(dim[0],dim[1]);
      }
      for (int i=0; i<2; ++i) info.m_angles[i]=(int) input->readLong(4);
      for (int i=0; i<3; ++i) info.m_n[i]=(int) input->readULong(2);
      info.m_orthoForm=(int) input->readULong(1);
      f << "infoRec=[" << info << "],";
    }
  }
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectEdge: find extra data\n"));
      f << "###extra,vers=" << vers;
    }
    ascFile.addDelimiter(input->tell(),'|');
  }
  if (pos!=lastPos) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectHeader(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphic &graphic)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SVDR)[" << zone.getRecordLevel() << "]:header,";

  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectHeader: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  long lastPos=zone.getRecordLastPosition();
  int vers=zone.getHeaderVersion();
  // svx_svdobj: SdrObject::ReadData
  int dim[4];    // gen.cxx operator>>(Rect) : test compression here
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(4);
  graphic.m_bdbox=STOFFBox2i(STOFFVec2i(dim[0],dim[1]),STOFFVec2i(dim[2],dim[3]));
  graphic.m_layerId=(int) input->readULong(2);
  for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
  graphic.m_anchorPosition=STOFFVec2i(dim[0],dim[1]);
  for (int i=0; i<5; ++i) *input >> graphic.m_flags[i];
  if (vers>=4) *input >> graphic.m_flags[5];
  bool ok=true;
  if (input->tell()>lastPos) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectHeader: oops read to much data\n"));
    f << "###bad,";
    ok=false;
  }
  if (ok && vers<11) {
    // poly.cxx operator>>(Polygon) : test compression here
    uint16_t n;
    *input >> n;
    if (input->tell()+8*n>lastPos) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectHeader: the number of point seems bad\n"));
      f << "###n=" << n << ",";
      ok=false;
      n=0;
    }
    for (int pt=0; pt<int(n); ++pt) {
      for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
      graphic.m_polygon.push_back(StarObjectSmallGraphicInternal::GluePoint(dim[0],dim[1]));
    }
  }
  if (ok && vers>=11) {
    bool bTmp;
    *input >> bTmp;
    if (bTmp) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      pos=input->tell();
      f.str("");
      f << "SVDR[headerB]:";
      if (!readSDRGluePointList(zone, graphic.m_polygon)) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectHeader: can not find the gluePoints record\n"));
        f << "###gluePoint";
        ok=false;
      }
      else
        pos=input->tell();
    }
  }
  f << graphic;
  if (ok) {
    bool readUser=true;
    if (vers>=11) *input >> readUser;
    // TODO: store user data list
    if (readUser) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      pos=input->tell();
      f.str("");
      f << "SVDR[headerC]:";
      if (!readSDRUserDataList(zone, vers>=11)) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectHeader: can not find the data list record\n"));
        f << "###dataList";
      }
      else
        pos=input->tell();
    }
  }

  if (input->tell()!=pos) {
    if (input->tell()!=lastPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  zone.closeRecord("SVDR");
  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectGraph(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicGraph &graphic)
{
  if (!readSVDRObjectRect(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:";
  // SdrGrafObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraph: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  int vers=zone.getHeaderVersion();
  bool ok=true;
  if (vers<11) {
    // ReadDataTilV10
    StarGraphicStruct::StarGraphic smallGraphic;
    if (!smallGraphic.read(zone) || input->tell()>lastPos) {
      f << "###graphic";
      ok=false;
    }
    else if (smallGraphic.m_bitmap)
      graphic.m_bitmap=smallGraphic.m_bitmap;
    if (ok && vers>=6) {
      int dim[4];
      for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(4);
      graphic.m_graphRectangle=STOFFBox2i(STOFFVec2i(dim[0],dim[1]),STOFFVec2i(dim[2],dim[3]));
    }
    if (ok && vers>=8) {
      std::vector<uint32_t> string;
      if (!zone.readString(string) || input->tell()>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraph: can not read the file name\n"));
        f << "###fileName";
        ok=false;
      }
      else
        graphic.m_graphNames[1]=libstoff::getString(string);
    }
    if (ok && vers>=9) {
      std::vector<uint32_t> string;
      if (!zone.readString(string) || input->tell()>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraph: can not read the filter name\n"));
        f << "###filter";
        ok=false;
      }
      else
        graphic.m_graphNames[2]=libstoff::getString(string);
    }
  }
  else {
    bool hasGraphic;
    *input >> hasGraphic;
    if (hasGraphic) {
      if (!zone.openRecord()) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraph: can not open graphic record\n"));
        f << "###graphRecord";
        ok=false;
      }
      else {
        f << "graf,";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        StarGraphicStruct::StarGraphic smallGraphic;
        if (!smallGraphic.read(zone, zone.getRecordLastPosition()) || input->tell()>zone.getRecordLastPosition()) {
          ascFile.addPos(pos);
          ascFile.addNote("SVDR[graph]:##graphic");
          input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
        }
        else if (smallGraphic.m_bitmap)
          graphic.m_bitmap=smallGraphic.m_bitmap;
        pos=input->tell();
        f.str("");
        f << "SVDR[graph]:";
        zone.closeRecord("SVDR");
      }
    }
    if (ok) {
      int dim[4];
      for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(4);
      graphic.m_graphRectangle=STOFFBox2i(STOFFVec2i(dim[0],dim[1]),STOFFVec2i(dim[2],dim[3]));
      *input >> graphic.m_mirrored;
      for (int i=0; i<3; ++i) {
        std::vector<uint32_t> string;
        if (!zone.readString(string) || input->tell()>lastPos) {
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraph: can not read a string\n"));
          f << "###string";
          ok=false;
          break;
        }
        graphic.m_graphNames[i]=libstoff::getString(string);
      }
    }
    if (ok)
      *input >> graphic.m_hasGraphicLink;
    if (ok && input->tell()<lastPos) {
      shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
      if (!pool)
        pool=getNewItemPool(StarItemPool::T_XOutdevPool);
      uint16_t nWhich=1243; // SDRATTRSET_GRAF
      shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
      if (!item || input->tell()>lastPos) {
        f << "###";
      }
      else
        graphic.m_graphItem=item;
    }
  }
  f << graphic;
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGraphic: find extra data\n"));
      f << "###extra";
    }
    ascFile.addDelimiter(input->tell(),'|');
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");
  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectGroup(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicGroup &graphic)
{
  if (!readSVDRObjectHeader(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:";
  // svx_svdogrp SdrObjGroup::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGroup: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  int vers=zone.getHeaderVersion();
  std::vector<uint32_t> string;
  bool ok=true;
  if (!zone.readString(string) || input->tell()>lastPos) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGroup: can not read the name\n"));
    ok=false;
  }
  else if (!string.empty())
    graphic.m_groupName=libstoff::getString(string);
  if (ok) {
    *input >> graphic.m_hasRefPoint;
    int dim[2];
    for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
    graphic.m_refPoint=STOFFVec2i(dim[0],dim[1]);
    if (input->tell()>lastPos) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGroup: the zone seems too short\n"));
      f << "###short";
    }
  }
  f << graphic;
  while (ok && input->tell()+4<lastPos) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    f.str("");
    f << "SVDR:group,";
    pos=input->tell();
    // check magic
    std::string magic("");
    for (int i=0; i<4; ++i) magic+=(char) input->readULong(1);
    input->seek(-4, librevenge::RVNG_SEEK_CUR);
    if (magic=="DrXX" && zone.openSDRHeader(magic)) {
      ascFile.addPos(pos);
      ascFile.addNote("SVDR:DrXX");
      zone.closeSDRHeader("SVDR");
      pos=input->tell();
      break;
    }
    if (magic!="DrOb")
      break;
    shared_ptr<StarObjectSmallGraphic> child(new StarObjectSmallGraphic(*this, true));
    if (!child->readSdrObject(zone)) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGroup: can not read an object\n"));
      f << "###object";
      ok=false;
      break;
    }
    graphic.m_child.push_back(child);
  }
  if (ok && vers>=2) {
    graphic.m_groupDrehWink=(int) input->readLong(4);
    if (graphic.m_groupDrehWink)
      f << "drehWink=" << graphic.m_groupDrehWink << ",";
    graphic.m_groupShearWink=(int) input->readLong(4);
    if (graphic.m_groupShearWink)
      f << "shearWink=" << graphic.m_groupShearWink << ",";
  }
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectGroup: find extra data\n"));
      f << "###extra";
    }
    if (input->tell()!=pos)
      ascFile.addDelimiter(input->tell(),'|');
  }
  if (pos!=lastPos) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectMeasure(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicMeasure &graphic)
{
  if (!readSVDRObjectText(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  // svx_svdomeas SdrMeasureObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectMeasure: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  for (int pt=0; pt<2; ++pt) {
    int dim[2];
    for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
    graphic.m_measurePoints[pt]=STOFFVec2i(dim[0],dim[1]);
  }
  *input >> graphic.m_overwritten;
  shared_ptr<StarItemPool> pool=findItemPool(StarItemPool::T_XOutdevPool, false);
  if (!pool)
    pool=getNewItemPool(StarItemPool::T_XOutdevPool);
  uint16_t nWhich=1171; // SDRATTRSET_MEASURE
  shared_ptr<StarItem> item=pool->loadSurrogate(zone, nWhich, false, f);
  if (!item || input->tell()>lastPos) {
    f << "###";
  }
  else
    graphic.m_measureItem=item;
  zone.closeRecord("SVDR");

  std::string extra=f.str();
  f.str("");
  f << "SVDR[" << zone.getRecordLevel() << "]:" << graphic;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectOLE(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicOLE &graphic)
{
  if (!readSVDRObjectRect(zone, graphic))
    return false;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  // svx_svdoole2 SdrOle2Obj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectOLE: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  bool ok=true;
  for (int i=0; i<2; ++i) {
    std::vector<uint32_t> string;
    if (!zone.readString(string) || input->tell()>lastPos) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectOLE: can not read a string\n"));
      f << "###string";
      ok=false;
      break;
    }
    if (!string.empty())
      graphic.m_oleNames[i]=libstoff::getString(string);
  }
  if (ok) {
    bool objValid, hasGraphic;
    *input >> objValid >> hasGraphic;
    if (objValid) f << "obj[refValid],";
    if (hasGraphic) {
      StarGraphicStruct::StarGraphic smallGraphic;
      if (!smallGraphic.read(zone, lastPos) || input->tell()>lastPos) {
        // TODO: we can recover here the unknown graphic
        f << "###graphic";
        ok=false;
      }
      else
        graphic.m_bitmap=smallGraphic.m_bitmap;
    }
  }
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectOLE: find extra data\n"));
      f << "###extra";
    }
    ascFile.addDelimiter(input->tell(),'|');
  }
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");

  std::string extra=f.str();
  f.str("");
  f << "SVDR[" << zone.getRecordLevel() << "]:" << graphic << extra;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectPath(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicPath &graphic)
{
  if (!readSVDRObjectText(zone, graphic))
    return false;
  int const &id=graphic.m_identifier;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  int vers=zone.getHeaderVersion();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:";
  // svx_svdopath SdrPathObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectPath: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  bool ok=true;
  if (vers<=6 && (id==2 || id==8 || id==9)) {
    int nPoly=id==2 ? 2 : id==8 ? 1 : (int) input->readULong(2);
    for (int poly=0; poly<nPoly; ++poly) {
      uint16_t n;
      *input >> n;
      if (input->tell()+8*n>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectPath: the number of point seems bad\n"));
        f << "###n=" << n << ",";
        ok=false;
        break;
      }
      for (int pt=0; pt<int(n); ++pt) {
        int dim[2];
        for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
        graphic.m_pathPolygon.push_back(STOFFVec2i(dim[0],dim[1]));
      }
    }
  }
  else {
    bool recOpened=false;
    if (vers>=11) {
      recOpened=zone.openRecord();
      if (!recOpened) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectPath: can not open zone record\n"));
        ok=false;
      }
    }
    int nPoly=ok ? (int) input->readULong(2) : 0;
    for (int poly=0; poly<nPoly; ++poly) {
      uint16_t n;
      *input >> n;
      if (input->tell()+9*n>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectPath: the number of point seems bad\n"));
        f << "###n=" << n << ",";
        ok=false;
        break;
      }
      for (int pt=0; pt<int(n); ++pt) {
        int dim[2];
        for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
        graphic.m_pathPolygon.push_back(STOFFVec2i(dim[0],dim[1]));
      }
      for (int pt=0; pt<int(n); ++pt) graphic.m_pathPolygonFlags.push_back((int) input->readULong(1));
    }
    if (recOpened) {
      if (input->tell()!=zone.getRecordLastPosition()) {
        if (ok) {
          f << "##";
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectPath: find extra data\n"));
        }
        ascFile.addDelimiter(input->tell(),'|');
      }
      input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
      zone.closeRecord("SVDR");
    }
    ok=false;
  }
  if (!ok) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  }
  zone.closeRecord("SVDR");
  f << graphic;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectRect(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicRect &graphic)
{
  if (!readSVDRObjectText(zone, graphic))
    return false;
  int const &id=graphic.m_identifier;
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  int vers=zone.getHeaderVersion();
  if (vers<3 && (id==16 || id==17 || id==20 || id==21))
    return true;

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:rectZone,";
  // svx_svdorect.cxx SdrRectObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectRect: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (vers<=5) graphic.m_eckRag=(int) input->readLong(4);
  f << graphic;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeRecord("SVDR");
  return true;
}

bool StarObjectSmallGraphic::readSVDRObjectText(StarZone &zone, StarObjectSmallGraphicInternal::SdrGraphicText &graphic)
{
  if (!readSVDRObjectAttrib(zone, graphic))
    return false;

  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "SVDR[" << zone.getRecordLevel() << "]:textZone,";
  // svx_svdotext SdrTextObj::ReadData
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectText: can not open record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long lastPos=zone.getRecordLastPosition();
  int vers=zone.getHeaderVersion();
  graphic.m_textKind=(int) input->readULong(1);
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(4);
  graphic.m_textRectangle=STOFFBox2i(STOFFVec2i(dim[0],dim[1]),STOFFVec2i(dim[2],dim[3]));
  graphic.m_textDrehWink=(int) input->readLong(4);
  graphic.m_textShearWink=(int) input->readLong(4);
  f << graphic;
  bool paraObjectValid;
  *input >> paraObjectValid;
  bool ok=input->tell()<=lastPos;
  if (paraObjectValid) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "SVDR:textB";
    if (vers>=11 && !zone.openRecord()) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectText: can not open paraObject record\n"));
      paraObjectValid=ok=false;
      f << "##paraObject";
    }
    else {
      shared_ptr<StarObjectSmallGraphicInternal::OutlinerParaObject> paraObject(new StarObjectSmallGraphicInternal::OutlinerParaObject);
      if (!readSDROutlinerParaObject(zone, *paraObject)) {
        ok=false;
        f << "##paraObject";
      }
      else {
        graphic.m_outlinerParaObject=paraObject;
        pos=input->tell();
      }
    }
    if (paraObjectValid && vers>=11) {
      zone.closeRecord("SdrParaObject");
      ok=true;
    }
  }
  if (ok && vers>=10) {
    bool hasBound;
    *input >> hasBound;
    if (hasBound) {
      for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(4);
      graphic.m_textBound=STOFFBox2i(STOFFVec2i(dim[0],dim[1]),STOFFVec2i(dim[2],dim[3]));
      f << "bound=" << graphic.m_textBound << ",";
    }
    ok=input->tell()<=lastPos;
  }
  if (input->tell()!=lastPos) {
    if (ok) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSVDRObjectText: find extra data\n"));
      f << "###extra, vers=" << vers;
    }
    ascFile.addDelimiter(input->tell(),'|');
  }
  if (pos!=input->tell()) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  zone.closeRecord("SVDR");
  return true;
}

bool StarObjectSmallGraphic::readSDRObjectConnection(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  // first check magic
  std::string magic("");
  long pos=input->tell();
  for (int i=0; i<4; ++i) magic+=(char) input->readULong(1);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (magic!="DrCn" || !zone.openSDRHeader(magic)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  long lastPos=zone.getRecordLastPosition();
  f << "Entries(SdrObjConn)[" << zone.getRecordLevel() << "]:";
  // svx_svdoedge.cxx SdrObjConnection::Read
  int version=zone.getHeaderVersion();
  f << magic << ",nVers=" << version << ",";
  if (!readSDRObjectSurrogate(zone)) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObjectConnection: can not read object surrogate\n"));
    f << "###surrogate";
    ascFile.addPos(input->tell());
    ascFile.addNote("SdrObjConn:###extra");
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
    zone.closeSDRHeader("SdrObjConn");
    return true;
  }
  f << "condId=" << input->readULong(2) << ",";
  f << "dist=" << input->readLong(4) << "x" << input->readLong(4) << ",";
  for (int i=0; i<6; ++i) {
    bool val;
    *input>>val;
    char const *(wh[])= {"bestConn", "bestVertex", "xDistOvr", "yDistOvr", "autoVertex", "autoCorner"};
    if (val)
      f << wh[i] << ",";
  }
  input->seek(8, librevenge::RVNG_SEEK_CUR);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (input->tell()!=lastPos) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObjectConnection: find extra data\n"));
    ascFile.addPos(input->tell());
    ascFile.addNote("SdrObjConn:###extra");
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  }
  zone.closeSDRHeader("SdrObjConn");
  return true;
}

bool StarObjectSmallGraphic::readSDRObjectSurrogate(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  long lastPos=zone.getRecordLastPosition();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrObjSurr):";
  // svx_svdsuro.cxx SdrObjSurrogate::ImpRead
  int id=(int) input->readULong(1);
  f << "id=" << id << ",";
  bool ok=true;
  if (id) {
    int eid=id&0x1f;
    int nBytes=1+(id>>6);
    if (nBytes==3) {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObjectConnection: unexpected num bytes\n"));
      f << "###nBytes,";
      ok=false;
    }
    if (ok)
      f << "val=" << input->readULong(nBytes) << ",";
    if (ok && eid>=0x10 && eid<=0x1a)
      f << "page=" << input->readULong(2) << ",";
    if (ok && id&0x20) {
      int grpLevel=(int) input->readULong(2);
      f << "nChild=" << grpLevel << ",";
      if (input->tell()+nBytes*grpLevel>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSdrObjectConnection: num child is bas\n"));
        f << "###";
        ok=false;
      }
      else {
        f << "child=[";
        for (int i=0; i<grpLevel; ++i)
          f << input->readULong(nBytes) << ",";
        f << "],";
      }
    }
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return ok && input->tell()<=lastPos;
}

bool StarObjectSmallGraphic::readSDROutlinerParaObject(StarZone &zone, StarObjectSmallGraphicInternal::OutlinerParaObject &object)
{
  object=StarObjectSmallGraphicInternal::OutlinerParaObject();
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  long lastPos=zone.getRecordLastPosition();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrParaObject):";
  // svx_outlobj.cxx OutlinerParaObject::Create
  long N=(long) input->readULong(4);
  f << "N=" << N << ",";
  long syncRef=(long) input->readULong(4);
  int vers=0;
  if (syncRef == 0x12345678)
    vers = 1;
  else if (syncRef == 0x22345678)
    vers = 2;
  else if (syncRef == 0x32345678)
    vers = 3;
  else if (syncRef == 0x42345678)
    vers = 4;
  else {
    f << "##syncRef,";
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDROutlinerParaObject: can not check the version\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return N==0;
  }
  object.m_version=vers;
  f << "version=" << vers << ",";
  if (vers<=3) {
    for (long i=0; i<N; ++i) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      pos=input->tell();
      f.str("");
      f << "SdrParaObject:";
      shared_ptr<StarObjectSmallText> smallText(new StarObjectSmallText(*this, true));
      if (!smallText->read(zone, lastPos) || input->tell()>lastPos) {
        f << "###editTextObject";
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return false;
      }
      pos=input->tell();
      StarObjectSmallGraphicInternal::OutlinerParaObject::Zone paraZone;
      paraZone.m_text=smallText;
      f << "sync=" << input->readULong(4) << ",";
      paraZone.m_depth=(int) input->readULong(2);
      bool ok=true;
      if (vers==1) {
        int flags=(int) input->readULong(2);
        if (flags&1) {
          StarBitmap bitmap;
          librevenge::RVNGBinaryData data;
          std::string dType;
          if (!bitmap.readBitmap(zone, true, lastPos, data, dType)) {
            STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDROutlinerParaObject: can not check the bitmpa\n"));
            ok=false;
          }
          else
            paraZone.m_background.add(data, dType);
        }
        else {
          if (!input->readColor(paraZone.m_backgroundColor)) {
            STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDROutlinerParaObject: can not find a color\n"));
            f << "###aColor,";
            ok=false;
          }
          else
            input->seek(16, librevenge::RVNG_SEEK_CUR);
          std::vector<uint32_t> string;
          if (ok && (!zone.readString(string) || input->tell()>lastPos)) {
            STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDROutlinerParaObject: can not find string\n"));
            f << "###string,";
            ok=false;
          }
          else
            paraZone.m_colorName=libstoff::getString(string);
          if (ok)
            input->seek(12, librevenge::RVNG_SEEK_CUR);
        }
        input->seek(8, librevenge::RVNG_SEEK_CUR); // 2 long dummy
      }
      f << paraZone;
      if (input->tell()>lastPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDROutlinerParaObject: we read too much data\n"));
        f << "###bad,";
        ok=false;
      }
      if (!ok) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return false;
      }
      object.m_zones.push_back(paraZone);
      if (i+1!=N) f << "sync=" << input->readULong(4) << ",";
    }
    if (vers==3) {
      *input >> object.m_isEditDoc;
      if (object.m_isEditDoc) f << "isEditDoc,";
    }
  }
  else {
    pos=input->tell();
    f.str("");
    f << "SdrParaObject:";
    shared_ptr<StarObjectSmallText> smallText(new StarObjectSmallText(*this, true)); // checkme, we must use the text edit pool here
    if (!smallText->read(zone, lastPos) || input->tell()+N*2>lastPos) {
      f << "###editTextObject";
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    object.m_textZone=smallText;
    pos=input->tell();
    f << "depth=[";
    for (long i=0; i<N; ++i) {
      object.m_depthList.push_back((int) input->readULong(2));
      f << object.m_depthList.back() << ",";
    }
    f << "],";
    *input >> object.m_isEditDoc;
    if (object.m_isEditDoc) f << "isEditDoc,";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool StarObjectSmallGraphic::readSDRGluePoint(StarZone &zone, StarObjectSmallGraphicInternal::GluePoint &pt)
{
  pt=StarObjectSmallGraphicInternal::GluePoint();
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  if (!zone.openRecord()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrGluePoint):";
  // svx_svdglue_drawdoc.xx: operator>>(SdrGluePoint)
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readULong(2);
  pt.m_dimension=STOFFVec2i(dim[0],dim[1]);
  pt.m_direction=(int) input->readULong(2);
  pt.m_id=(int) input->readULong(2);
  pt.m_align=(int) input->readULong(2);
  bool noPercent;
  *input >> noPercent;
  pt.m_percent=!noPercent;
  f << "pt,";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeRecord("SdrGluePoint");
  return true;
}

bool StarObjectSmallGraphic::readSDRGluePointList
(StarZone &zone, std::vector<StarObjectSmallGraphicInternal::GluePoint> &listPoints)
{
  listPoints.clear();
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  if (!zone.openRecord()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrGluePoint)[list]:";
  // svx_svdglue_drawdoc.xx: operator>>(SdrGluePointList)
  int n=(int) input->readULong(2);
  f << "n=" << n << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<n; ++i) {
    pos=input->tell();
    StarObjectSmallGraphicInternal::GluePoint pt;
    if (!readSDRGluePoint(zone, pt)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRGluePointList: can not find a glue point\n"));
    }
    listPoints.push_back(pt);
  }
  zone.closeRecord("SdrGluePoint");
  return true;
}

bool StarObjectSmallGraphic::readSDRUserData(StarZone &zone, bool inRecord)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  if (inRecord && !zone.openRecord()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrUserData):";
  // svx_svdobj.xx: SdrObject::ReadData
  long lastPos=zone.getRecordLastPosition();
  if (input->tell()+6>lastPos) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserData: the zone seems too short\n"));
  }
  else {
    std::string type("");
    for (int i=0; i<4; ++i) type+=(char) input->readULong(1);
    int id=(int) input->readULong(2);
    f << type << ",id=" << id << ",";
    if (type=="SCHU" || type=="SDUD") {
      if ((type=="SCHU" && !readSCHUObject(zone, id)) || (type=="SDUD" && !readSDUDObject(zone, id))) {
        f << "##";
        if (!inRecord) {
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserData: can not determine end size\n"));
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          return false;
        }
      }
      else if (!inRecord)
        lastPos=input->tell();
    }
    else {
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserData: find unknown type=%s\n", type.c_str()));
      f << "###";
      static bool first=true;
      if (first) {
        first=false;
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserData: reading data is not implemented\n"));
      }
      if (!inRecord) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserData: can not determine end size\n"));
        f << "##";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return false;
      }
    }
  }
  if (input->tell()!=lastPos) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (inRecord)
    zone.closeRecord("SdrUserData");
  return true;
}

bool StarObjectSmallGraphic::readSDRUserDataList(StarZone &zone, bool inRecord)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  if (inRecord && !zone.openRecord()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SdrUserData)[list]:";
  // svx_svdglue_drawdoc.xx: operator>>(SdrUserDataList)
  int n=(int) input->readULong(2);
  f << "n=" << n << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<n; ++i) {
    pos=input->tell();
    if (!readSDRUserData(zone, inRecord)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDRUserDataList: can not find a glue point\n"));
    }
  }
  if (inRecord) zone.closeRecord("SdrUserData");
  return true;
}

////////////////////////////////////////////////////////////
// FM01 object
////////////////////////////////////////////////////////////
shared_ptr<StarObjectSmallGraphicInternal::Graphic> StarObjectSmallGraphic::readFmFormObject(StarZone &zone, int identifier)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(FM01):";

  if (identifier!=33) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readFmFormObject: find unknown identifier\n"));
    f << "###id=" << identifier << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  // svx_fmobj.cxx FmFormObj::ReadData
  // fixme: same code as SdrUnoObj::ReadData
  shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicUno> graphic(new StarObjectSmallGraphicInternal::SdrGraphicUno());
  if (!readSVDRObjectRect(zone, *graphic)) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readFmFormObject: can not read rect data\n"));
    f << "###id=" << identifier << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  pos=input->tell();
  if (!zone.openRecord()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readFmFormObject: can not open uno record\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###id=" << identifier << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  f << "FM01[uno]:";
  // + SdrUnoObj::ReadData (checkme)
  std::vector<uint32_t> string;
  bool ok=true;
  if (input->tell()!=zone.getRecordLastPosition() && (!zone.readString(string) || input->tell()>zone.getRecordLastPosition())) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readFmFormObject: can not read uno string\n"));
    f << "###uno";
    ok=false;
  }
  else
    graphic->m_unoName=libstoff::getString(string);
  f << *graphic;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (!ok)
    input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
  zone.closeRecord("FM01");
  return graphic;
}

////////////////////////////////////////////////////////////
// SCHU object
////////////////////////////////////////////////////////////
shared_ptr<StarObjectSmallGraphicInternal::Graphic> StarObjectSmallGraphic::readSCHUObject(StarZone &zone, int identifier)
{
  if (identifier==1) {
    shared_ptr<StarObjectSmallGraphicInternal::SdrGraphicGroup> group(new StarObjectSmallGraphicInternal::SdrGraphicGroup(1));
    if (readSVDRObjectGroup(zone, *group))
      return group;
  }
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(SCHU):";
  if (identifier<=0 || identifier>7) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSCHUObject: find unknown identifier\n"));
    f << "###id=" << identifier << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  shared_ptr<StarObjectSmallGraphicInternal::SCHUGraphic> graphic(new StarObjectSmallGraphicInternal::SCHUGraphic(identifier));
  // sch_objfac.xx : SchObjFactory::MakeUserData
  int vers=(int) input->readULong(2);
  switch (identifier) {
  case 2:
  case 7:
    graphic->m_id=(int) input->readULong(2);
    break;
  case 3:
    graphic->m_adjust=(int) input->readULong(2);
    if (vers>=1)
      graphic->m_orientation=(int) input->readULong(2);
    break;
  case 4:
    graphic->m_row=(int) input->readLong(2);
    break;
  case 5:
    graphic->m_column=(int) input->readLong(2);
    graphic->m_row=(int) input->readLong(2);
    break;
  case 6:
    *input >> graphic->m_factor;
    break;
  default:
    f << "##";
    break;
  }
  f << *graphic;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return graphic;
}

////////////////////////////////////////////////////////////
// SCHU object
////////////////////////////////////////////////////////////
shared_ptr<StarObjectSmallGraphicInternal::Graphic> StarObjectSmallGraphic::readSDUDObject(StarZone &zone, int identifier)
{
  STOFFInputStreamPtr input=zone.input();
  long pos=input->tell();

  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  if (identifier<=0 || identifier>2) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: find unknown identifier\n"));
    f << "Entries(SDUD):###id=" << identifier << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  // sd_sdobjfac.cxx : SchObjFactory::MakeUserData
  int vers=(int) input->readULong(2);
  f << "vers=" << vers << ",";
  if (!zone.openSCHHeader()) {
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: can not open main record\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return shared_ptr<StarObjectSmallGraphicInternal::Graphic>();
  }
  vers=zone.getHeaderVersion();
  long endPos=zone.getRecordLastPosition();
  shared_ptr<StarObjectSmallGraphicInternal::SDUDGraphic> res;
  if (identifier==1) {
    // sd_anminfo.cxx SdAnimationInfo::ReadData
    shared_ptr<StarObjectSmallGraphicInternal::SDUDGraphicAnimation> graphic(new StarObjectSmallGraphicInternal::SDUDGraphicAnimation);
    res=graphic;
    bool ok=true;
    if (input->readULong(2)) {
      uint16_t n;
      *input >> n;
      if (input->tell()+8*n>endPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: the number of point seems bad\n"));
        f << "###n=" << n << ",";
        ok=false;
      }
      else {
        for (int pt=0; pt<int(n); ++pt) {
          int dim[2];
          for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
          graphic->m_polygon.push_back(STOFFVec2i(dim[0],dim[1]));
        }
      }
    }
    if (ok) {
      for (int pt=0; pt<2; ++pt) {
        int dim[2];
        for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(4);
        graphic->m_limits[pt]=STOFFVec2i(dim[0],dim[1]);
      }
      for (int i=0; i<2; ++i)
        graphic->m_values[i]=(int) input->readULong(2);
      for (int i=0; i<3; ++i) graphic->m_flags[i]=input->readULong(2)!=0;
      if (input->tell()>endPos) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: the zone is too short\n"));
        f << "###short";
        ok=false;
      }
    }
    if (ok) {
      for (int i=0; i<2; ++i) {
        STOFFColor color;
        if (!input->readColor(color) || input->tell()>endPos) {
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: can not find a color\n"));
          f << "###aColor,";
          ok=false;
          break;
        }
        else
          graphic->m_colors[i]=color;
      }
    }
    int encoding=0;
    if (ok && vers>0) {
      encoding=int(input->readULong(2));
      std::vector<uint32_t> string;
      if (ok && (!zone.readString(string, encoding) || input->tell()>endPos)) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: can not find string\n"));
        f << "###string,";
        ok=false;
      }
      else
        graphic->m_names[0]=libstoff::getString(string);
    }
    if (ok && vers>1)
      *input >> graphic->m_booleans[0];
    if (ok && vers>2)
      *input >> graphic->m_booleans[1];
    if (ok && vers>3) {
      int nFlag=(int) input->readULong(2);
      if (nFlag==1) {
        // TODO store surrogate
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        pos=input->tell();
        f.str("");
        f << "SDUD-B:";
        if (!readSDRObjectSurrogate(zone) || input->tell()>endPos) {
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: can not read object surrogate\n"));
          f << "###surrogate";
          ok=false;
        }
        else
          pos=input->tell();
      }
    }
    if (ok && vers>4) {
      for (int i=2; i<5; ++i)
        graphic->m_values[i]=(int) input->readULong(2);
      for (int i=1; i<3; ++i) {
        std::vector<uint32_t> string;
        if (ok && (!zone.readString(string, encoding) || input->tell()>endPos)) {
          STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: can not find string\n"));
          f << "###string,";
          ok=false;
          break;
        }
        graphic->m_names[i]=libstoff::getString(string);
      }
      if (ok) {
        for (int i=5; i<7; ++i)
          graphic->m_values[i]=(int) input->readULong(2);
      }
    }
    if (ok && vers>5)
      *input >> graphic->m_booleans[2] >> graphic->m_booleans[3];
    if (ok && vers>6)
      *input >> graphic->m_booleans[4];
    if (ok && vers>7)
      graphic->m_values[7]=(int) input->readULong(2);
    if (ok && vers>8)
      graphic->m_order=(int) input->readULong(4);
    if (input->tell()!=endPos) {
      ascFile.addDelimiter(input->tell(),'|');
      if (ok) {
        STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: find extra data\n"));
        f << "###";
      }
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
    }
    std::string extra=f.str();
    f.str("");
    f << "Entries(SDUD):" << *graphic << extra;
  }
  else {
    res.reset(new StarObjectSmallGraphicInternal::SDUDGraphic(identifier));
    f.str("");
    f << "Entries(SDUD):imageMap,###";
    // imap2.cxx ImageMap::Read ; never seen, complex, so...
    STOFF_DEBUG_MSG(("StarObjectSmallGraphic::readSDUDObject: reading imageMap is not implemented\n"));
    f << "###";
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSCHHeader("SDUD");
  return res;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: