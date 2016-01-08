/******************************************************************************
 * Copyright (c) 2015 Thomas Faeulhammer, Aitor Aldoma
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


#ifndef NMBasedCloudIntegration_H
#define NMBasedCloudIntegration_H

#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/octree/octree_pointcloud_pointvector.h>
#include <pcl/octree/impl/octree_iterator.hpp>

#include <v4r/core/macros.h>
#include <v4r/common/miscellaneous.h>

namespace v4r
{

/**
 * @brief reconstructs a point cloud from several input clouds. Each point of the input cloud is associated with a weight
 * which states the measurement confidence ( 0... max noise level, 1... very confident). Each point is accumulated into a
 * big cloud and then reprojected into the various image planes of the input clouds to check for conflicting points.
 * Conflicting points will be removed and the remaining points put into an octree
 * @author Thomas Faeulhammer, Aitor Aldoma
 * @date December 2015
 */
template<class PointT>
class V4R_EXPORTS NMBasedCloudIntegration
{
public:
    class V4R_EXPORTS Parameter
    {
    public:
        int min_points_per_voxel_;  /// @brief the minimum number of points in a leaf of the octree of the big cloud.
        float octree_resolution_;   /// @brief resolution of the octree of the big point cloud
        float focal_length_;   /// @brief focal length of the cameras; used for reprojection of the points into each image plane
        bool average_;  /// @brief if true, takes the average color (for each color componenent) and normal within all the points in the leaf of the octree. Otherwise, it takes the point within the octree with the best noise weight
        float threshold_explained_; /// @brief Euclidean distance for a nearby point to explain a query point. Only used if reason_about_points = true
        bool reason_about_points_; /// @brief if true, projects each point into each viewpoint and checks for occlusion or if it can be explained by the points in the other viewpoints (this should filter lonely points but is computational expensive) --> FILTER NOT IMPLEMENTED SO FAR!!
        float edge_radius_px_; /// @brief points of the input cloud within this distance (in pixel) to its closest depth discontinuity pixel will be removed
        Parameter(
                int min_points_per_voxel = 0,
                float octree_resolution =  0.005f,
                float focal_length = 525.f,
                bool average = false,
                float threshold_explained = 0.02f,
                bool reason_about_points = false,
                float edge_radius_px = 2.f) :
            min_points_per_voxel_(min_points_per_voxel),
            octree_resolution_(octree_resolution),
            focal_length_ (focal_length),
            average_ (average),
            threshold_explained_ (threshold_explained),
            reason_about_points_ (reason_about_points),
            edge_radius_px_ (edge_radius_px)
        {
        }
    }param_;

private:
    typedef typename pcl::PointCloud<PointT>::Ptr PointTPtr;
    typedef typename pcl::PointCloud<pcl::Normal>::Ptr PointNormalTPtr;

    struct PointInfo{
        PointT pt;
        pcl::Normal normal;

        // for each point store the number of viewpoints in which the point
            //  [0] is occluded;
            //  [1] can be explained by a nearby point;
            //  [2] could be seen but does not make sense (i.e. the view-ray is not blocked, but there is no sensed point)
        size_t occluded_, explained_, violated_;
        int origin;
        float distance_to_depth_discontinuity;
        float sigma_lateral;
        float sigma_axial;
        float probability;

        bool operator<(const PointInfo other) const
        {
            return probability > other.probability;
        }

        PointInfo()
        {
            occluded_  = violated_ = 0;
            explained_ = 1; // the point is explained at least by the original viewpoint
        }
    };

    std::vector<PointInfo> big_cloud_info_;
    std::vector<PointTPtr> input_clouds_;
    std::vector<PointNormalTPtr> input_normals_;
    std::vector<std::vector<size_t> > indices_; /// @brief Indices of the object in each cloud (remaining points will be ignored)
    std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > transformations_to_global_; /// @brief transform aligning the input point clouds when multiplied
    std::vector<std::vector<std::vector<float> > > pt_properties_; /// @brief for each cloud, for each pixel represent lateral [idx=0] and axial [idx=1] as well as distance to closest depth discontinuity [idx=2]
    PointNormalTPtr output_normals_;

    void cleanUp()
    {
        input_clouds_.clear();
        input_normals_.clear();
        indices_.clear();
        transformations_to_global_.clear();
        pt_properties_.clear();
        big_cloud_info_.clear();
    }

    void collectInfo();
    void reasonAboutPts();

public:
    NMBasedCloudIntegration (const Parameter &p=Parameter()) : param_(p)
    {
    }

    /**
     * @brief getOutputNormals
     * @param Normals of the registered cloud
     */
    void getOutputNormals(PointNormalTPtr & output) const
    {
        output = output_normals_;
    }

    /**
     * @brief setInputClouds
     * @param organized input clouds
     */
    void
    setInputClouds (const std::vector<PointTPtr> & input)
    {
        if ( input.size() < 2)
            std::cerr << "There are not enough point clouds to do a noise model based cloud integration. I need at least two!" << std::endl;

        input_clouds_ = input;
    }

    /**
     * @brief setInputNormals
     * @param normal clouds corresponding to the input clouds
     */
    void
    setInputNormals (const std::vector<PointNormalTPtr> & input)
    {
        input_normals_ = input;
    }

    /**
     * @brief setIndices
     * @param object mask
     */
    void
    setIndices(const std::vector<std::vector<size_t> > & indices)
    {
        indices_ = indices;
    }


    void
    setIndices(const std::vector<std::vector<int> > & indices)
    {
        indices_.resize(indices.size());
        for(size_t i=0; i<indices.size(); i++)
            indices_[i] = convertVecInt2VecSizet(indices[i]);
    }

    /**
     * @brief compute the registered point cloud taking into account the noise model of the cameras
     * @param registered cloud
     */
    void
    compute (PointTPtr &output);

    /**
     * @brief setPointProperties
     * @param for each cloud, for each pixel represent lateral [idx=0] and axial [idx=1] as well as distance to closest depth discontinuity [idx=2]
     */
    void
    setPointProperties (const std::vector<std::vector<std::vector<float> > > & pt_properties)
    {
        pt_properties_ = pt_properties;
    }

    /**
     * @brief setTransformations
     * @param transforms aligning each point cloud to a global coordinate system
     */
    void setTransformations(const std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > & transforms)
    {
        transformations_to_global_ = transforms;
    }
};
}

#endif /* NOISE_MODELS_H_ */
