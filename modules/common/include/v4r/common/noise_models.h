/*
 * noise_models.h
 *
 *  Created on: Oct 28, 2013
 *      Author: aitor
 */

#ifndef NOISE_MODELS_H_
#define NOISE_MODELS_H_

#include <pcl/common/common.h>
#include <pcl/common/io.h>

#include <v4r/core/macros.h>

namespace v4r
{
    namespace noise_models
    {
      template<class PointT>
        class V4R_EXPORTS NguyenNoiseModel
        {
        private:
          typedef typename pcl::PointCloud<PointT>::Ptr PointTPtr;
          typedef typename pcl::PointCloud<pcl::Normal>::Ptr PointNormalTPtr;
          PointTPtr input_;
          PointNormalTPtr normals_;
          std::vector<float> weights_;
          pcl::PointIndices discontinuity_edges_;
          Eigen::Matrix4f pose_to_plane_RF_;
          bool pose_set_;

        public:
          struct nguyensNoiseModelParams
          {
              float lateral_sigma_;
              float max_angle_;
              bool use_depth_edges_;
          }nguyens_noise_model_params_;

          NguyenNoiseModel ();

          //this is the pose used to align a cloud so that its aligned to the RF
          //defined on a plane (z-axis corresponds to the plane normal) and
          //the origin is on the plane

          void setPoseToPlaneRF(Eigen::Matrix4f & pose)
          {
              pose_to_plane_RF_ = pose;
              pose_set_ = true;
          }

          void
          setInputCloud (const PointTPtr & input)
          {
            input_ = input;
          }

          void
          setMaxAngle(float f)
          {
            nguyens_noise_model_params_.max_angle_ = f;
          }

          void
          setUseDepthEdges(bool b)
          {
            nguyens_noise_model_params_.use_depth_edges_ = b;
          }

          void
          getDiscontinuityEdges(pcl::PointCloud<pcl::PointXYZ>::Ptr & disc) const
          {
            disc.reset(new pcl::PointCloud<pcl::PointXYZ>);
            pcl::copyPointCloud(*input_, discontinuity_edges_, *disc);
          }

          //in meters, lateral sigma (3*s used to downweight points close to depth discontinuities)
          void
          setLateralSigma(float s)
          {
            nguyens_noise_model_params_.lateral_sigma_ = s;
          }

          void
          setInputNormals (const PointNormalTPtr & normals)
          {
            normals_ = normals;
          }

          void
          compute ();

          void
          getWeights (std::vector<float> & weights) const
          {
            weights = weights_;
          }

          //void getFilteredCloud(PointTPtr & filtered, float w_t);

          void getFilteredCloudRemovingPoints(PointTPtr & filtered, float w_t);

          void getFilteredCloudRemovingPoints(PointTPtr & filtered, float w_t, std::vector<int> & kept);
        };
    }
}

#endif /* NOISE_MODELS_H_ */

