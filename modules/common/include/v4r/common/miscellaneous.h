/******************************************************************************
 * Copyright (c) 2015 Thomas Faeulhammer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ******************************************************************************/

/**
*
*      @author Thomas Faeulhammer (faeulhammer@acin.tuwien.ac.at)
*      @date July, 2015
*      @brief some commonly used functions
*/

#ifndef V4R_COMMON_MISCELLANEOUS_H_
#define V4R_COMMON_MISCELLANEOUS_H_

#include <pcl/common/common.h>
#include <pcl/kdtree/flann.h>
#include <pcl/octree/octree.h>
#include <pcl/octree/octree_pointcloud_pointvector.h>
#include <pcl/octree/impl/octree_iterator.hpp>
#include <v4r/core/macros.h>

namespace v4r
{

V4R_EXPORTS inline void transformNormals(const pcl::PointCloud<pcl::Normal> & normals_cloud,
                             pcl::PointCloud<pcl::Normal> & normals_aligned,
                             const Eigen::Matrix4f & transform)
{
    normals_aligned.points.resize (normals_cloud.points.size ());
    normals_aligned.width = normals_cloud.width;
    normals_aligned.height = normals_cloud.height;
    for (size_t k = 0; k < normals_cloud.points.size (); k++)
    {
        Eigen::Vector3f nt (normals_cloud.points[k].normal_x, normals_cloud.points[k].normal_y, normals_cloud.points[k].normal_z);
        normals_aligned.points[k].normal_x = static_cast<float> (transform (0, 0) * nt[0] + transform (0, 1) * nt[1]
                + transform (0, 2) * nt[2]);
        normals_aligned.points[k].normal_y = static_cast<float> (transform (1, 0) * nt[0] + transform (1, 1) * nt[1]
                + transform (1, 2) * nt[2]);
        normals_aligned.points[k].normal_z = static_cast<float> (transform (2, 0) * nt[0] + transform (2, 1) * nt[1]
                + transform (2, 2) * nt[2]);

        normals_aligned.points[k].curvature = normals_cloud.points[k].curvature;

    }
}

V4R_EXPORTS inline void transformNormals(const pcl::PointCloud<pcl::Normal> & normals_cloud,
                             pcl::PointCloud<pcl::Normal> & normals_aligned,
                             const std::vector<int> & indices,
                             const Eigen::Matrix4f & transform)
{
    normals_aligned.points.resize (indices.size ());
    normals_aligned.width = indices.size();
    normals_aligned.height = 1;
    for (size_t k = 0; k < indices.size(); k++)
    {
        Eigen::Vector3f nt (normals_cloud.points[indices[k]].normal_x,
                normals_cloud.points[indices[k]].normal_y,
                normals_cloud.points[indices[k]].normal_z);

        normals_aligned.points[k].normal_x = static_cast<float> (transform (0, 0) * nt[0] + transform (0, 1) * nt[1]
                + transform (0, 2) * nt[2]);
        normals_aligned.points[k].normal_y = static_cast<float> (transform (1, 0) * nt[0] + transform (1, 1) * nt[1]
                + transform (1, 2) * nt[2]);
        normals_aligned.points[k].normal_z = static_cast<float> (transform (2, 0) * nt[0] + transform (2, 1) * nt[1]
                + transform (2, 2) * nt[2]);

        normals_aligned.points[k].curvature = normals_cloud.points[indices[k]].curvature;

    }
}

V4R_EXPORTS inline void transformNormal(const Eigen::Vector3f & nt,
                            Eigen::Vector3f & normal_out,
                            const Eigen::Matrix4f & transform)
{
    normal_out[0] = static_cast<float> (transform (0, 0) * nt[0] + transform (0, 1) * nt[1] + transform (0, 2) * nt[2]);
    normal_out[1] = static_cast<float> (transform (1, 0) * nt[0] + transform (1, 1) * nt[1] + transform (1, 2) * nt[2]);
    normal_out[2] = static_cast<float> (transform (2, 0) * nt[0] + transform (2, 1) * nt[1] + transform (2, 2) * nt[2]);
}

/**
 * @brief Returns homogenous 4x4 transformation matrix for given rotation (quaternion) and translation components
 * @param q rotation represented as quaternion
 * @param trans homogenous translation
 * @return tf 4x4 homogeneous transformation matrix
 *
 */
V4R_EXPORTS inline Eigen::Matrix4f
RotTrans2Mat4f(const Eigen::Quaternionf &q, const Eigen::Vector4f &trans)
{
    Eigen::Matrix4f tf = Eigen::Matrix4f::Identity();;
    tf.block<3,3>(0,0) = q.toRotationMatrix();
    tf.block<4,1>(0,3) = trans;
    tf(3,3) = 1.f;
    return tf;
}


/**
 * @brief Returns homogenous 4x4 transformation matrix for given rotation (quaternion) and translation components
 * @param q rotation represented as quaternion
 * @param trans translation
 * @return tf 4x4 homogeneous transformation matrix
 *
 */
V4R_EXPORTS inline Eigen::Matrix4f
RotTrans2Mat4f(const Eigen::Quaternionf &q, const Eigen::Vector3f &trans)
{
    Eigen::Matrix4f tf = Eigen::Matrix4f::Identity();
    tf.block<3,3>(0,0) = q.toRotationMatrix();
    tf.block<3,1>(0,3) = trans;
    return tf;
}

/**
 * @brief Returns rotation (quaternion) and translation components from a homogenous 4x4 transformation matrix
 * @param tf 4x4 homogeneous transformation matrix
 * @param q rotation represented as quaternion
 * @param trans homogenous translation
 */
inline void
V4R_EXPORTS Mat4f2RotTrans(const Eigen::Matrix4f &tf, Eigen::Quaternionf &q, Eigen::Vector4f &trans)
{
    Eigen::Matrix3f rotation = tf.block<3,3>(0,0);
    q = rotation;
    trans = tf.block<4,1>(0,3);
}

V4R_EXPORTS inline void voxelGridWithOctree(pcl::PointCloud<pcl::PointXYZRGB>::Ptr & cloud,
                                pcl::PointCloud<pcl::PointXYZRGB> & voxel_grided,
                                float resolution)
{
    pcl::octree::OctreePointCloudPointVector<pcl::PointXYZRGB> octree(resolution);
    octree.setInputCloud(cloud);
    octree.addPointsFromInputCloud();

    pcl::octree::OctreePointCloudPointVector<pcl::PointXYZRGB>::LeafNodeIterator it2;
    const pcl::octree::OctreePointCloudPointVector<pcl::PointXYZRGB>::LeafNodeIterator it2_end = octree.leaf_end();

    int leaves = 0;
    for (it2 = octree.leaf_begin(); it2 != it2_end; ++it2, leaves++)
    {

    }

    voxel_grided.points.resize(leaves);
    voxel_grided.width = leaves;
    voxel_grided.height = 1;
    voxel_grided.is_dense = true;

    int kk=0;
    for (it2 = octree.leaf_begin(); it2 != it2_end; ++it2, kk++)
    {
        pcl::octree::OctreeContainerPointIndices& container = it2.getLeafContainer();
        std::vector<int> indexVector;
        container.getPointIndices (indexVector);

        int r,g,b;
        r = g = b = 0;
        pcl::PointXYZRGB p;
        p.getVector3fMap() = Eigen::Vector3f::Zero();

        for(size_t k=0; k < indexVector.size(); k++)
        {
            p.getVector3fMap() = p.getVector3fMap() +  cloud->points[indexVector[k]].getVector3fMap();
            r += cloud->points[indexVector[k]].r;
            g += cloud->points[indexVector[k]].g;
            b += cloud->points[indexVector[k]].b;
        }

        p.getVector3fMap() = p.getVector3fMap() / static_cast<int>(indexVector.size());
        p.r = r / static_cast<int>(indexVector.size());
        p.g = g / static_cast<int>(indexVector.size());
        p.b = b / static_cast<int>(indexVector.size());
        voxel_grided.points[kk] = p;
    }
}


/**
 * @brief returns point indices from a point cloud which are closest to search points
 * @param full_input_cloud
 * @param search_points
 * @param indices
 * @param resolution (optional)
 */
template<typename PointInT>
V4R_EXPORTS inline void
getIndicesFromCloud(const typename pcl::PointCloud<PointInT>::ConstPtr & full_input_cloud,
                    const typename pcl::PointCloud<PointInT>::ConstPtr & search_points,
                    std::vector<int> & indices,
                    float resolution = 0.005f)
{
    pcl::octree::OctreePointCloudSearch<PointInT> octree (resolution);
    octree.setInputCloud (full_input_cloud);
    octree.addPointsFromInputCloud ();

    std::vector<int> pointIdxNKNSearch;
    std::vector<float> pointNKNSquaredDistance;

    indices.resize( search_points->points.size() );
    size_t kept=0;

    for(size_t j=0; j < search_points->points.size(); j++)
    {
        if (octree.nearestKSearch (search_points->points[j], 1, pointIdxNKNSearch, pointNKNSquaredDistance) > 0)
        {
            indices[kept] = pointIdxNKNSearch[0];
            kept++;
        }
    }
    indices.resize(kept);
}

/**
 * @brief returns point indices from a point cloud which are closest to search points
 * @param full_input_cloud
 * @param search_points
 * @param indices
 * @param resolution (optional)
 */
template<typename PointT, typename Type>
V4R_EXPORTS inline void
getIndicesFromCloud(const typename pcl::PointCloud<PointT>::ConstPtr & full_input_cloud,
                    const typename pcl::PointCloud<PointT> & search_pts,
                    typename std::vector<Type> & indices,
                    float resolution = 0.005f)
{
    pcl::octree::OctreePointCloudSearch<PointT> octree (resolution);
    octree.setInputCloud (full_input_cloud);
    octree.addPointsFromInputCloud ();

    std::vector<int> pointIdxNKNSearch;
    std::vector<float> pointNKNSquaredDistance;

    indices.resize( search_pts.points.size() );
    size_t kept=0;

    for(size_t j=0; j < search_pts.points.size(); j++)
    {
        if (octree.nearestKSearch (search_pts.points[j], 1, pointIdxNKNSearch, pointNKNSquaredDistance) > 0)
        {
            indices[kept] = pointIdxNKNSearch[0];
            kept++;
        }
    }
    indices.resize(kept);
}

template<typename DistType>
V4R_EXPORTS void convertToFLANN ( const std::vector<std::vector<float> > &data, boost::shared_ptr< typename flann::Index<DistType> > &flann_index);

template<typename DistType>
V4R_EXPORTS void nearestKSearch ( typename boost::shared_ptr< flann::Index<DistType> > &index, std::vector<float> descr, int k, flann::Matrix<int> &indices,
                                                  flann::Matrix<float> &distances );

/**
 * @brief sets the sensor origin and sensor orientation fields of the PCL pointcloud header by the given transform
 */
template<typename PointType> V4R_EXPORTS void setCloudPose(const Eigen::Matrix4f &trans, typename pcl::PointCloud<PointType> &cloud);

V4R_EXPORTS inline std::vector<size_t>
convertVecInt2VecSizet(const std::vector<int> &input)
{
    std::vector<size_t> v_size_t;
    v_size_t.resize(input.size());
    for (size_t i=0; i<input.size(); i++)
    {
        if(input[i] < 0)
            std::cerr << "Casting a negative integer to unsigned type size_t!" << std::endl;

        v_size_t[i] = static_cast<size_t>( input[i] );
    }
    return v_size_t;
}

V4R_EXPORTS inline std::vector<int>
convertVecSizet2VecInt(const std::vector<size_t> &input)
{
    std::vector<int> v_int;
    v_int.resize(input.size());
    for (size_t i=0; i<input.size(); i++)
    {
        if ( input[i] > static_cast<size_t>(std::numeric_limits<int>::max()) )
            std::cerr << "Casting an unsigned type size_t with a value larger than limits of integer!" << std::endl;

        v_int[i] = static_cast<int>( input[i] );
    }
    return v_int;
}

V4R_EXPORTS inline pcl::PointIndices
convertVecSizet2PCLIndices(const std::vector<size_t> &input)
{
    pcl::PointIndices pind;
    pind.indices.resize(input.size());
    for (size_t i=0; i<input.size(); i++)
    {
        if ( input[i] > static_cast<size_t>(std::numeric_limits<int>::max()) )
            std::cerr << "Casting an unsigned type size_t with a value larger than limits of integer!" << std::endl;

        pind.indices[i] = static_cast<int>( input[i] );
    }
    return pind;
}

V4R_EXPORTS inline std::vector<size_t>
convertPCLIndices2VecSizet(const pcl::PointIndices &input)
{
    std::vector<size_t> v_size_t;
    v_size_t.resize(input.indices.size());
    for (size_t i=0; i<input.indices.size(); i++)
    {
        if(input.indices[i] < 0)
            std::cerr << "Casting a negative integer to unsigned type size_t!" << std::endl;

        v_size_t[i] = static_cast<size_t>( input.indices[i] );
    }
    return v_size_t;
}

V4R_EXPORTS inline std::vector<bool>
createMaskFromIndices(const std::vector<size_t> &indices,size_t image_size)
{
    std::vector<bool> mask (image_size, false);

    for (size_t obj_pt_id = 0; obj_pt_id < indices.size(); obj_pt_id++)
        mask [ indices[obj_pt_id] ] = true;

    return mask;
}


V4R_EXPORTS inline std::vector<bool>
createMaskFromIndices(const std::vector<int> &indices, size_t image_size)
{
    std::vector<bool> mask (image_size, false);

    for (size_t obj_pt_id = 0; obj_pt_id < indices.size(); obj_pt_id++)
        mask [ indices[obj_pt_id] ] = true;

    return mask;
}


template<typename T>
V4R_EXPORTS std::vector<T>
createIndicesFromMask(const std::vector<bool> &mask, bool invert=false)
{
    std::vector<T> out;
    out.resize(mask.size());

    size_t kept=0;
    for(size_t i=0; i<mask.size(); i++)
    {
        if( ( mask[i] && !invert ) || ( !mask[i] && invert ))
        {
            out[kept] = static_cast<T>(i);
            kept++;
        }
    }
    out.resize(kept);
    return out;
}

/**
  * @brief: Increments a boolean vector by 1 (LSB at the end)
  * @param v Input vector
  * @param inc_v Incremented output vector
  * @return overflow (true if overflow)
  */
inline V4R_EXPORTS bool
incrementVector(const std::vector<bool> &v, std::vector<bool> &inc_v)
{
    inc_v = v;

    bool overflow=true;
    for(size_t bit=0; bit<v.size(); bit++)
    {
        if(!v[bit])
        {
            overflow = false;
            break;
        }
    }

    bool carry = v.back();
    inc_v.back() = !v.back();
    for(int bit=v.size()-2; bit>=0; bit--)
    {
        inc_v[bit] = v[ bit ] != carry;
        carry = v[ bit ] && carry;
    }
    return overflow;
}

/**
  * @brief: extracts elements from a vector indicated by some indices
  * @param[in] Input vector
  * @param[in] indices to extract
  * @return extracted elements
  */
template<typename T>
inline V4R_EXPORTS typename std::vector<T>
filterVector(const std::vector<T> &in, const std::vector<int> &indices)
{
    typename std::vector<T> out(in.size());
    size_t kept;
    for(const auto &idx : indices)
        out[kept++] = in[idx];

    out.resize(kept);
    return out;
}
}


namespace pcl
{
/** \brief Extract the indices of a given point cloud as a new point cloud (instead of int types, this function uses a size_t vector)
  * \param[in] cloud_in the input point cloud dataset
  * \param[in] indices the vector of indices representing the points to be copied from \a cloud_in
  * \param[out] cloud_out the resultant output point cloud dataset
  * \note Assumes unique indices.
  * \ingroup common
  */
template <typename PointT> V4R_EXPORTS void
copyPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                const std::vector<size_t> &indices,
                pcl::PointCloud<PointT> &cloud_out);

/** \brief Extract the indices of a given point cloud as a new point cloud (instead of int types, this function uses a size_t vector)
  * \param[in] cloud_in the input point cloud dataset
  * \param[in] indices the vector of indices representing the points to be copied from \a cloud_in
  * \param[out] cloud_out the resultant output point cloud dataset
  * \note Assumes unique indices.
  * \ingroup common
  */
template <typename PointT> V4R_EXPORTS void
copyPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                const std::vector<size_t, Eigen::aligned_allocator<size_t> > &indices,
                pcl::PointCloud<PointT> &cloud_out);

template <typename PointT> V4R_EXPORTS void
copyPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                     const std::vector<bool> &mask,
                     pcl::PointCloud<PointT> &cloud_out);
}

#endif
