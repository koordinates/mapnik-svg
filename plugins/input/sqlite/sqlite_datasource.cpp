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

#include "sqlite_datasource.hpp"
#include "sqlite_featureset.hpp"

// mapnik
#include <mapnik/ptree_helpers.hpp>
#include <mapnik/sql_utils.hpp>

// boost
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem/operations.hpp>

using boost::lexical_cast;
using boost::bad_lexical_cast;

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(sqlite_datasource)

using mapnik::box2d;
using mapnik::coord2d;
using mapnik::query;
using mapnik::featureset_ptr;
using mapnik::layer_descriptor;
using mapnik::attribute_descriptor;
using mapnik::datasource_exception;


sqlite_datasource::sqlite_datasource(parameters const& params, bool bind)
   : datasource(params),
     extent_(),
     extent_initialized_(false),
     type_(datasource::Vector),
     table_(*params_.get<std::string>("table","")),
     fields_(*params_.get<std::string>("fields","*")),
     metadata_(*params_.get<std::string>("metadata","")),
     geometry_table_(*params_.get<std::string>("geometry_table","")),
     geometry_field_(*params_.get<std::string>("geometry_field","the_geom")),
     key_field_(*params_.get<std::string>("key_field","OGC_FID")),
     row_offset_(*params_.get<int>("row_offset",0)),
     row_limit_(*params_.get<int>("row_limit",0)),
     desc_(*params_.get<std::string>("type"), *params_.get<std::string>("encoding","utf-8")),
     format_(mapnik::wkbGeneric)
{
    boost::optional<std::string> file = params_.get<std::string>("file");
    if (!file) throw datasource_exception("Sqlite Plugin: missing <file> parameter");

    boost::optional<std::string> wkb = params_.get<std::string>("wkb_format");
    if (wkb)
    {
        if (*wkb == "spatialite")
            format_ = mapnik::wkbSpatiaLite;  
    }

    multiple_geometries_ = *params_.get<mapnik::boolean>("multiple_geometries",false);
    use_spatial_index_ = *params_.get<mapnik::boolean>("use_spatial_index",true);

    boost::optional<std::string> ext  = params_.get<std::string>("extent");
    if (ext) extent_initialized_ = extent_.from_string(*ext);

    boost::optional<std::string> base = params_.get<std::string>("base");
    if (base)
        dataset_name_ = *base + "/" + *file;
    else
        dataset_name_ = *file;

    if (bind)
    {
        this->bind();
    }
}

void sqlite_datasource::bind() const
{
    if (is_bound_) return;
    
    if (!boost::filesystem::exists(dataset_name_))
        throw datasource_exception("Sqlite Plugin: " + dataset_name_ + " does not exist");
          
    dataset_ = new sqlite_connection (dataset_name_);

    if(geometry_table_.empty())
    {
        geometry_table_ = mapnik::table_from_sql(table_);
    }
    
    if (metadata_ != "" && !extent_initialized_)
    {
        std::ostringstream s;
        s << "SELECT xmin, ymin, xmax, ymax FROM " << metadata_;
        s << " WHERE LOWER(f_table_name) = LOWER('" << geometry_table_ << "')";
        boost::scoped_ptr<sqlite_resultset> rs (dataset_->execute_query (s.str()));
        if (rs->is_valid () && rs->step_next())
        {
            double xmin = rs->column_double (0);
            double ymin = rs->column_double (1);
            double xmax = rs->column_double (2);
            double ymax = rs->column_double (3);

            extent_.init (xmin,ymin,xmax,ymax);
            extent_initialized_ = true;
        }
    }

    if (use_spatial_index_)
    {
        std::ostringstream s;
        s << "SELECT COUNT (*) FROM sqlite_master";
        s << " WHERE LOWER(name) = LOWER('idx_" << geometry_table_ << "_" << geometry_field_ << "')";
        boost::scoped_ptr<sqlite_resultset> rs (dataset_->execute_query (s.str()));
        if (rs->is_valid () && rs->step_next())
        {
            use_spatial_index_ = rs->column_integer (0) == 1;
        }

#ifdef MAPNIK_DEBUG
        if (! use_spatial_index_)
           std::clog << "Sqlite Plugin: cannot use the spatial index " << std::endl;
#endif
    }
    
    {
        /*
            XXX - This is problematic, if we do not have at least a row,
                  we cannot determine the right columns types and names 
                  as all column_type are SQLITE_NULL
        */

        std::ostringstream s;
        s << "SELECT " << fields_ << " FROM (" << geometry_table_ << ") LIMIT 1";

        boost::scoped_ptr<sqlite_resultset> rs (dataset_->execute_query (s.str()));
        if (rs->is_valid () && rs->step_next())
        {
            for (int i = 0; i < rs->column_count (); ++i)
            {
               const int type_oid = rs->column_type (i);
               const char* fld_name = rs->column_name (i);
               switch (type_oid)
               {
                  case SQLITE_INTEGER:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Integer));
                     break;
                     
                  case SQLITE_FLOAT:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Double));
                     break;
                     
                  case SQLITE_TEXT:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));
                     break;
                     
                  case SQLITE_NULL:
                  case SQLITE_BLOB:
                     break;
                     
                  default:
#ifdef MAPNIK_DEBUG
                     std::clog << "Sqlite Plugin: unknown type_oid=" << type_oid << std::endl;
#endif
                     break;
                }
            }    
        }
    }
    
    is_bound_ = true;
}

sqlite_datasource::~sqlite_datasource()
{
    if (is_bound_) 
    {
        delete dataset_;
    }
}

std::string sqlite_datasource::name()
{
   return "sqlite";
}

int sqlite_datasource::type() const
{
   return type_;
}

box2d<double> sqlite_datasource::envelope() const
{
   if (!is_bound_) bind();
   return extent_;
}

layer_descriptor sqlite_datasource::get_descriptor() const
{
   if (!is_bound_) bind();
   return desc_;
}

featureset_ptr sqlite_datasource::features(query const& q) const
{
   if (!is_bound_) bind();
   if (dataset_)
   {
        mapnik::box2d<double> const& e = q.get_bbox();

        std::ostringstream s;
        
        s << "SELECT " << geometry_field_ << "," << key_field_;
        std::set<std::string> const& props = q.property_names();
        std::set<std::string>::const_iterator pos = props.begin();
        std::set<std::string>::const_iterator end = props.end();
        while (pos != end)
        {
           s << "," << *pos << "";
           ++pos;
        }
        
        s << " FROM "; 
        
        std::string query (table_); 
        
        if (use_spatial_index_)
        {
           std::ostringstream spatial_sql;
           spatial_sql << std::setprecision(16);
           // TODO - rowid must be explicitly included in subselects
           // should we use primary key here instead?
           spatial_sql << " WHERE rowid IN (SELECT pkid FROM idx_" << geometry_table_ << "_" << geometry_field_;
           spatial_sql << "  WHERE xmax>=" << e.minx() << " AND xmin<=" << e.maxx() ;
           spatial_sql << "    AND ymax>=" << e.miny() << " AND ymin<=" << e.maxy() << ")";
           if (boost::algorithm::ifind_first(query, "WHERE"))
           {
              boost::algorithm::ireplace_first(query, "WHERE", spatial_sql.str() + " AND ");
           }
           else if (boost::algorithm::ifind_first(query, geometry_table_))  
           {
              boost::algorithm::ireplace_first(query, table_, table_ + " " + spatial_sql.str());
           }
        }
        
        s << query ;
        
        if (row_limit_ > 0) {
            s << " LIMIT " << row_limit_;
        }

        if (row_offset_ > 0) {
            s << " OFFSET " << row_offset_;
        }

#ifdef MAPNIK_DEBUG
        std::clog << "Sqlite Plugin: " << s.str() << std::endl;
#endif

        boost::shared_ptr<sqlite_resultset> rs (dataset_->execute_query (s.str()));

        return featureset_ptr (new sqlite_featureset(rs, desc_.get_encoding(), format_, multiple_geometries_));
   }

   return featureset_ptr();
}

featureset_ptr sqlite_datasource::features_at_point(coord2d const& pt) const
{
   if (!is_bound_) bind();

   return featureset_ptr();
}

