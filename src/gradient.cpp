/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2010 Artem Pavelenko
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

#include <mapnik/gradient.hpp>
#include <iostream>

namespace mapnik
{

static const char * gradient_strings[] = {
    "no-gradient",
    "linear",
    "radial",
    /*
    "radial_focus",
    "x",
    "y",
    "xy",
    "sqrt_xy",
    "diamond",
    "conic",
    */
    ""
};

IMPLEMENT_ENUM( gradient_e, gradient_strings );


gradient::gradient() 
    : gradient_type_(NO_GRADIENT),
      stops_() {}

gradient::gradient(gradient const& other)
    : gradient_type_(other.gradient_type_),
      stops_(other.stops_) {}

gradient & gradient::operator=(const gradient& rhs)
{
    gradient tmp(rhs);
    swap(tmp);
    return *this;
}

void gradient::set_gradient_type(gradient_e grad) 
{
    gradient_type_=grad;
}

gradient_e gradient::get_gradient_type() const 
{
    return gradient_type_;
}

void gradient::add_stop(double offset,mapnik::color const& c)
{
    stops_.push_back(mapnik::stop_pair(offset,c));
}

bool gradient::has_stop() const 
{
    return ! stops_.empty();
}

stop_array const& gradient::get_stop_array() const
{
    return stops_;
}

void gradient::swap(const gradient& other) throw()
{
    gradient_type_=other.gradient_type_;
    stops_=other.stops_;
}
}
