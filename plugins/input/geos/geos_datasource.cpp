/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2007 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/
// $Id$

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdarg>

#include "geos_datasource.hpp"
#include "geos_featureset.hpp"

// mapnik
#include <mapnik/ptree_helpers.hpp>
#include <mapnik/geom_util.hpp>

// boost
#include <boost/algorithm/string.hpp>

// geos
#include <geos_c.h>

using std::clog;
using std::endl;

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(geos_datasource)

using mapnik::box2d;
using mapnik::coord2d;
using mapnik::query;
using mapnik::featureset_ptr;
using mapnik::layer_descriptor;
using mapnik::attribute_descriptor;
using mapnik::datasource_exception;
using mapnik::filter_in_box;
using mapnik::filter_at_point;


void notice(const char* fmt, ...)
{
    va_list ap;
    fprintf( stdout, "NOTICE: ");
    
    va_start (ap, fmt);
    vfprintf( stdout, fmt, ap);
    va_end(ap);
    fprintf( stdout, "\n" );
}

void log_and_exit(const char* fmt, ...)
{
    va_list ap;
    fprintf( stdout, "ERROR: ");

    va_start (ap, fmt);
    vfprintf( stdout, fmt, ap);
    va_end(ap);
    fprintf( stdout, "\n" );
    
    //exit(1);
}


geos_datasource::geos_datasource(parameters const& params, bool bind)
   : datasource(params),
     extent_(),
     type_(datasource::Vector),
     desc_(*params.get<std::string>("type"), *params.get<std::string>("encoding","utf-8"))
{
    boost::optional<std::string> geometry = params.get<std::string>("geometry");
    if (!geometry) throw datasource_exception("missing <geometry> parameter");
    geometry_ = *geometry;

    multiple_geometries_ = *params_.get<mapnik::boolean>("multiple_geometries",false);

    // initialize envelope
    //extent_.init (envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY);

    if (bind)
    {
        this->bind();
    }
}

geos_datasource::~geos_datasource()
{
    if (is_bound_) 
    {
        finishGEOS();
    }
}

void geos_datasource::bind() const
{
   if (is_bound_) return;   

   // open ogr driver
   initGEOS(notice, log_and_exit);
   
   is_bound_ = true;
}

std::string geos_datasource::name()
{
    return "geos";
}

int geos_datasource::type() const
{
    return type_;
}

box2d<double> geos_datasource::envelope() const
{
    if (!is_bound_) bind();
    return extent_;
}

layer_descriptor geos_datasource::get_descriptor() const
{
    if (!is_bound_) bind();
    return desc_;
}

featureset_ptr geos_datasource::features(query const& q) const
{
    if (!is_bound_) bind();

    const mapnik::box2d<double> extent = q.get_bbox();

    std::ostringstream s;
    s << "POLYGON("
      << extent.minx() << " " << extent.miny() << ","
      << extent.minx() << " " << extent.maxy() << ","
      << extent.maxx() << " " << extent.maxy() << ","
      << extent.maxx() << " " << extent.miny() << ","
      << extent.minx() << " " << extent.miny()
      << ")";

    return featureset_ptr(new geos_featureset (geometry_,
                                               s.str(),
                                               desc_.get_encoding(),
                                               multiple_geometries_));
}

featureset_ptr geos_datasource::features_at_point(coord2d const& pt) const
{
    if (!is_bound_) bind();

    std::ostringstream s;
    s << "POINT(" << pt.x << " " << pt.y << ")";

    return featureset_ptr(new geos_featureset (geometry_,
                                               s.str(),
                                               desc_.get_encoding(),
                                               multiple_geometries_));
}

