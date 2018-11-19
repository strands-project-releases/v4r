/**
 * $Id$
 * 
 * Software License Agreement (GNU General Public License)
 *
 *  Copyright (C) 2015:
 *
 *    Johann Prankl, prankl@acin.tuwien.ac.at
 *    Aitor Aldoma, aldoma@acin.tuwien.ac.at
 *
 *      Automation and Control Institute
 *      Vienna University of Technology
 *      Gusshausstraße 25-29
 *      1170 Vienn, Austria
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Johann Prankl
 *
 */

#ifndef KP_TSF_SURFEL_HH
#define KP_TSF_SURFEL_HH

#include <Eigen/Dense>
#include <pcl/point_types.h>
#include <v4r/core/macros.h>

namespace v4r
{



/**
   * @brief The Surfel class
   */
class V4R_EXPORTS Surfel
{
public:
  Eigen::Vector3f pt;
  Eigen::Vector3f n;
  float weight;
  float radius;
  int r, g, b;
  Surfel() : weight(0), radius(0) {}
  Surfel(const pcl::PointXYZRGB &_pt) : pt(_pt.getArray3fMap()), weight(1), radius(0), r(_pt.r), g(_pt.g), b(_pt.b) {
    if (!std::isnan(pt[0]) && !std::isnan(pt[1]) &&!std::isnan(pt[2])) {
      n = -pt.normalized();
    }
    else
    {
      n = Eigen::Vector3f(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN());
      weight = 0;
    }
  }
};



/*************************** INLINE METHODES **************************/

} //--END--

#endif

