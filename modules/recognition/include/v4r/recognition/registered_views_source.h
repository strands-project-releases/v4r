/******************************************************************************
 * Copyright (c) 2012 Aitor Aldoma
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

#ifndef V4R_REG_VIEWS_SOURCE_H_
#define V4R_REG_VIEWS_SOURCE_H_

#include "source.h"
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <v4r/core/macros.h>
#include <v4r/common/faat_3d_rec_framework_defines.h>
#include <v4r/io/eigen.h>
#include <v4r/io/filesystem.h>

namespace v4r
{
/**
     * \brief Data source class based on partial views from sensor.
     * In this case, the training data is obtained directly from a depth sensor.
     * The filesystem should contain pcd files (representing a view of an object in
     * camera coordinates) and each view needs to be associated with a txt file
     * containing a 4x4 matrix representing the transformation from camera coordinates
     * to a global object coordinates frame.
     * \author Aitor Aldoma
     */

template<typename Full3DPointT = pcl::PointXYZRGBNormal, typename PointInT = pcl::PointXYZRGB, typename OutModelPointT = pcl::PointXYZRGB>
class V4R_EXPORTS RegisteredViewsSource : public Source<PointInT>
{
private:
    typedef Source<PointInT> SourceT;
    typedef Model<OutModelPointT> ModelT;
    typedef boost::shared_ptr<ModelT> ModelTPtr;

    using SourceT::path_;
    using SourceT::models_;
    using SourceT::load_into_memory_;
    using SourceT::resolution_;
    using SourceT::view_prefix_;
    using SourceT::indices_prefix_;
    using SourceT::pose_prefix_;
    using SourceT::entropy_prefix_;


public:
    RegisteredViewsSource (float resolution = 0.001f)
    {
        resolution_ = resolution;
        load_into_memory_ = false;
    }

    void
    assembleModelFromViewsAndPoses(ModelT & model,
                                   std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > & poses,
                                   std::vector<pcl::PointIndices> & indices,
                                   typename pcl::PointCloud<PointInT>::Ptr &model_cloud)
    {
        for(size_t i=0; i < model.views_.size(); i++)
        {
            Eigen::Matrix4f inv = poses[i];
            inv = inv.inverse();

            typename pcl::PointCloud<PointInT>::Ptr global_cloud_only_indices(new pcl::PointCloud<PointInT>);
            pcl::copyPointCloud(*(model.views_[i]), indices[i], *global_cloud_only_indices);
            typename pcl::PointCloud<PointInT>::Ptr global_cloud(new pcl::PointCloud<PointInT>);
            pcl::transformPointCloud(*global_cloud_only_indices, *global_cloud, inv);
            *model_cloud += *global_cloud;
        }
    }

    void
    loadInMemorySpecificModel(ModelT &model);

    void
    loadModel (ModelT & model);

    /**
    * \brief Creates the model representation of the training set, generating views if needed
    */
    void
    generate ();
};
}

#endif
