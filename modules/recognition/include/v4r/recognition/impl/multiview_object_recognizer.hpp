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
*      @date August, 2015
*      @brief multiview object instance recognizer
*      Reference(s): Faeulhammer et al, ICRA 2015
*                    Faeulhammer et al, MVA 2015
*/

#include <math.h>       // atan2
#include <pcl/keypoints/sift_keypoint.h>
#include <pcl/recognition/cg/geometric_consistency.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/icp.h>
#include <pcl/search/kdtree.h>

#include <v4r/common/faat_3d_rec_framework_defines.h>
#include <v4r/common/normals.h>
#include <v4r/common/pcl_visualization_utils.h>
#include <v4r/recognition/hv_go_3D.h>
#include <v4r/registration/fast_icp_with_gc.h>
#include <v4r/recognition/multiview_object_recognizer.h>
#include <v4r/recognition/segmenter.h>
#include <v4r/segmentation/multiplane_segmentation.h>
#include <v4r/segmentation/segmentation_utils.h>

#include <boost/graph/kruskal_min_spanning_tree.hpp>

namespace v4r
{

template<typename PointT>
bool
MultiviewRecognizer<PointT>::calcSiftFeatures (const typename pcl::PointCloud<PointT>::Ptr &cloud_src,
                                               typename pcl::PointCloud<PointT>::Ptr &sift_keypoints,
                                               std::vector< int > &sift_keypoint_indices,
                                               pcl::PointCloud<FeatureT>::Ptr &sift_signatures,
                                               std::vector<float> &sift_keypoint_scales)
{
    pcl::PointIndices sift_keypoint_pcl_indices;

    if(!sift_signatures)
        sift_signatures.reset(new pcl::PointCloud<FeatureT>);

    if(!sift_keypoints)
        sift_keypoints.reset(new pcl::PointCloud<PointT>);

#ifdef HAVE_SIFTGPU
    SIFTLocalEstimation<PointT, FeatureT> estimator(sift_);
    bool ret = estimator.estimate (cloud_src, sift_keypoints, sift_signatures, sift_keypoint_scales);
#else
    (void)sift_keypoint_scales; //silences compiler warning of unused variable
    typename pcl::PointCloud<PointT>::Ptr processed_foo (new pcl::PointCloud<PointT>());

    OpenCVSIFTLocalEstimation<PointT, FeatureT > estimator;
    bool ret = estimator.estimate (cloud_src, processed_foo, sift_keypoints, sift_signatures);
#endif
    estimator.getKeypointIndices( sift_keypoint_pcl_indices );
    sift_keypoint_indices = sift_keypoint_pcl_indices.indices;
    return ret;
}



template<typename PointT>
void
MultiviewRecognizer<PointT>::estimateViewTransformationBySIFT(const pcl::PointCloud<PointT> &src_cloud,
                                                              const pcl::PointCloud<PointT> &dst_cloud,
                                                              const std::vector<int> &src_sift_keypoint_indices,
                                                              const std::vector<int> &dst_sift_keypoint_indices,
                                                              const pcl::PointCloud<FeatureT> &src_sift_signatures,
                                                              boost::shared_ptr< flann::Index<DistT> > &dst_flann_index,
                                                              std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > &transformations,
                                                              bool use_gc )
{
    const int K = 1;
    flann::Matrix<int> indices = flann::Matrix<int> ( new int[K], 1, K );
    flann::Matrix<float> distances = flann::Matrix<float> ( new float[K], 1, K );

    boost::shared_ptr< pcl::PointCloud<PointT> > pSiftKeypointsSrc (new pcl::PointCloud<PointT>);
    boost::shared_ptr< pcl::PointCloud<PointT> > pSiftKeypointsDst (new pcl::PointCloud<PointT>);
    pcl::copyPointCloud(src_cloud, src_sift_keypoint_indices, *pSiftKeypointsSrc );
    pcl::copyPointCloud(dst_cloud, dst_sift_keypoint_indices, *pSiftKeypointsDst);

    pcl::CorrespondencesPtr temp_correspondences ( new pcl::Correspondences );
    temp_correspondences->resize(pSiftKeypointsSrc->size ());

    for ( size_t keypointId = 0; keypointId < pSiftKeypointsSrc->points.size (); keypointId++ )
    {
        FeatureT searchFeature = src_sift_signatures[ keypointId ];
        int size_feat = sizeof ( searchFeature.histogram ) / sizeof ( float );
        nearestKSearch ( dst_flann_index, searchFeature.histogram, size_feat, K, indices, distances );

        pcl::Correspondence corr;
        corr.distance = distances[0][0];
        corr.index_query = keypointId;
        corr.index_match = indices[0][0];
        temp_correspondences->at(keypointId) = corr;
    }

    if(!use_gc)
    {
        typename pcl::registration::CorrespondenceRejectorSampleConsensus<PointT>::Ptr rej;
        rej.reset (new pcl::registration::CorrespondenceRejectorSampleConsensus<PointT> ());
        pcl::CorrespondencesPtr after_rej_correspondences (new pcl::Correspondences ());

        rej->setMaximumIterations (50000);
        rej->setInlierThreshold (0.02);
        rej->setInputTarget (pSiftKeypointsDst);
        rej->setInputSource (pSiftKeypointsSrc);
        rej->setInputCorrespondences (temp_correspondences);
        rej->getCorrespondences (*after_rej_correspondences);

        Eigen::Matrix4f refined_pose;
        transformations.push_back( rej->getBestTransformation () );
        pcl::registration::TransformationEstimationSVD<PointT, PointT> t_est;
        t_est.estimateRigidTransformation (*pSiftKeypointsSrc, *pSiftKeypointsDst, *after_rej_correspondences, refined_pose);
        transformations.back() = refined_pose;
    }
    else
    {
        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > new_transforms;
        pcl::GeometricConsistencyGrouping<pcl::PointXYZRGB, pcl::PointXYZRGB> gcg_alg;

        gcg_alg.setGCThreshold (15);
        gcg_alg.setGCSize (0.01);
        gcg_alg.setInputCloud(pSiftKeypointsSrc);
        gcg_alg.setSceneCloud(pSiftKeypointsDst);
        gcg_alg.setModelSceneCorrespondences(temp_correspondences);

        std::vector<pcl::Correspondences> clustered_corrs;
        gcg_alg.recognize(new_transforms, clustered_corrs);
        transformations.insert(transformations.end(), new_transforms.begin(), new_transforms.end());
    }
}

template<typename PointT>
float
MultiviewRecognizer<PointT>::calcEdgeWeightAndRefineTf (const typename pcl::PointCloud<PointT>::ConstPtr &cloud_src,
                                                        const typename pcl::PointCloud<PointT>::ConstPtr &cloud_dst,
                                                        Eigen::Matrix4f &refined_transform,
                                                        const Eigen::Matrix4f &transform)
{
    typename pcl::PointCloud<PointT>::Ptr cloud_src_wo_nan ( new pcl::PointCloud<PointT>());
    typename pcl::PointCloud<PointT>::Ptr cloud_dst_wo_nan ( new pcl::PointCloud<PointT>());

    pcl::PassThrough<PointT> pass;
    pass.setFilterLimits (0.f, 5.f);
    pass.setFilterFieldName ("z");
    pass.setInputCloud (cloud_src);
    pass.setKeepOrganized (true);
    pass.filter (*cloud_src_wo_nan);

    pcl::PassThrough<PointT> pass2;
    pass2.setFilterLimits (0.f, 5.f);
    pass2.setFilterFieldName ("z");
    pass2.setInputCloud (cloud_dst);
    pass2.setKeepOrganized (true);
    pass2.filter (*cloud_dst_wo_nan);

    float w_after_icp_ = std::numeric_limits<float>::max ();
    const float best_overlap_ = 0.75f;

    FastIterativeClosestPointWithGC<pcl::PointXYZRGB> icp;
    icp.setMaxCorrespondenceDistance ( 0.02f );
    icp.setInputSource ( cloud_src_wo_nan );
    icp.setInputTarget ( cloud_dst_wo_nan );
    icp.setUseNormals (true);
    icp.useStandardCG (true);
    icp.setNoCG(true);
    icp.setOverlapPercentage (best_overlap_);
    icp.setKeepMaxHypotheses (5);
    icp.setMaximumIterations (10);
    icp.align (transform);
    w_after_icp_ = icp.getFinalTransformation ( refined_transform );

    if ( w_after_icp_ < 0 || !pcl_isfinite ( w_after_icp_ ) )
        w_after_icp_ = std::numeric_limits<float>::max ();
    else
        w_after_icp_ = best_overlap_ - w_after_icp_;

    //    transform = icp_trans; // refined transformation
    return w_after_icp_;
}

template<typename PointT>
void
MultiviewRecognizer<PointT>::pruneGraph ()
{
    if( views_.size() > param_.max_vertices_in_graph_ ) {
        size_t lowest_vertex_id = std::numeric_limits<size_t>::max();

        typename std::map<size_t, View<PointT> >::const_iterator view_it;
        for (view_it = views_.begin(); view_it != views_.end(); ++view_it) {
             if( view_it->first < lowest_vertex_id)
                 lowest_vertex_id = view_it->first;
        }

        views_.erase(lowest_vertex_id);

        if (param_.compute_mst_) {
            std::pair<vertex_iter, vertex_iter> vp;
            for (vp = vertices(gs_); vp.first != vp.second; ++vp.first) {
                if (gs_[*vp.first] == lowest_vertex_id) {
                    clear_vertex(*vp.first, gs_);
                    remove_vertex(*vp.first, gs_); // iterator might be invalidated but we stop anyway
                    break;
                }
            }
        }
    }
}

template<typename PointT>
bool
MultiviewRecognizer<PointT>::computeAbsolutePose(CamConnect &e, bool is_first_edge)
{
    size_t src = e.source_id_;
    size_t trgt = e.target_id_;
    View<PointT> &src_tmp = views_[src];
    View<PointT> &trgt_tmp = views_[trgt];

    std::cout << "[" << src << "->" << trgt << "] with weight " << e.edge_weight_ << " by " << e.model_name_ << std::endl;

    if (is_first_edge) {
        src_tmp.has_been_hopped_ = true;
        src_tmp.absolute_pose_ = Eigen::Matrix4f::Identity();
        src_tmp.cumulative_weight_to_new_vrtx_ = 0.f;
        is_first_edge = false;
    }

    if(src_tmp.has_been_hopped_) {
        trgt_tmp.has_been_hopped_ = true;
        trgt_tmp.absolute_pose_ = src_tmp.absolute_pose_ * e.transformation_;
        trgt_tmp.cumulative_weight_to_new_vrtx_ = src_tmp.cumulative_weight_to_new_vrtx_ + e.edge_weight_;
    }
    else if (trgt_tmp.has_been_hopped_) {
        src_tmp.has_been_hopped_ = true;
        src_tmp.absolute_pose_ = trgt_tmp.absolute_pose_ * e.transformation_.inverse();
        src_tmp.cumulative_weight_to_new_vrtx_ = trgt_tmp.cumulative_weight_to_new_vrtx_ + e.edge_weight_;
    }
    else {
        std::cerr << "None of the vertices has been hopped yet!";
        return false;
    }

    return true;
}

template<typename PointT>
void
MultiviewRecognizer<PointT>::recognize ()
{
    if(!rr_)
        throw std::runtime_error("Single-View recognizer is not set. Please provide a recognizer to the multi-view recognition system!");

    std::cout << "=================================================================" << std::endl <<
                 "Started recognition for view " << id_ << " in scene " << scene_name_ <<
                 "=========================================================" << std::endl << std::endl;

    boost::shared_ptr< pcl::PointCloud<pcl::Normal> > scene_normals_f (new pcl::PointCloud<pcl::Normal> );

    if (!scene_ || scene_->width != 640 || scene_->height != 480)
        throw std::runtime_error("Size of input cloud is not 640x480, which is the only resolution currently supported by the verification framework.");

    View<PointT> vv;
    views_[id_] = vv;
    View<PointT> &v = views_[id_];

    v.id_ = id_;
    v.scene_ = scene_;
    v.transform_to_world_co_system_ = pose_;
    v.absolute_pose_ = pose_;

    computeNormals<PointT>(v.scene_, v.scene_normals_, param_.normal_computation_method_);

    if( param_.chop_z_ > 0 && std::isfinite(param_.chop_z_)) {
        for(size_t i=0; i <v.scene_->points.size(); i++) {
            PointT &pt = v.scene_->points[i];
            if( pt.z > param_.chop_z_) {
                pt.z = pt.x = pt.y = std::numeric_limits<float>::quiet_NaN();// keep it organized
                pt.r = pt.g = pt.b = 0.f;
            }
        }
    }
    else {
        v.scene_f_ = v.scene_;
        scene_normals_f = v.scene_normals_;
    }


    if (param_.compute_mst_) {
        if( param_.scene_to_scene_) {   // compute SIFT keypoints for the scene (since neighborhood of keypoint
                                        // matters for their SIFT descriptors, the descriptors are computed on the
                                        // original rather than on the filtered point cloud. Keypoints at infinity
                                        // are removed.
            typename pcl::PointCloud<PointT>::Ptr sift_keypoints (new pcl::PointCloud<PointT>());
            std::vector<int> sift_kp_indices;
            boost::shared_ptr< pcl::PointCloud<FeatureT > > sift_signatures_ (new  pcl::PointCloud<FeatureT>);
            std::vector<float> sift_keypoints_scales;

            calcSiftFeatures( v.scene_, sift_keypoints, sift_kp_indices, sift_signatures_, sift_keypoints_scales);

            if(!v.sift_signatures_)
                v.sift_signatures_.reset( new pcl::PointCloud<FeatureT>);

            v.sift_kp_indices_.indices.reserve( sift_kp_indices.size() );
            v.sift_signatures_->points.reserve( sift_signatures_->points.size() );
//            v.sift_keypoints_scales_.reserve( sift_keypoints_scales.size() );
            size_t kept=0;
            for (size_t i=0; i<sift_kp_indices.size(); i++) {   // remove infinte keypoints
                if ( pcl::isFinite( v.scene_->points[sift_kp_indices[i]] ) ) {
                    v.sift_kp_indices_.indices.push_back( sift_kp_indices[i] );
                    v.sift_signatures_->points.push_back( sift_signatures_->points[i] );
//                    v.sift_keypoints_scales_.push_back( sift_keypoints_scales[i] );
                    kept++;
                }
            }
            v.sift_kp_indices_.indices.shrink_to_fit();
            v.sift_signatures_->points.shrink_to_fit();
//            v.sift_keypoints_scales_.shrink_to_fit();
            std::cout << "keypoints: " << v.sift_kp_indices_.indices.size() << std::endl;

            // In addition to matching views, we can use the computed SIFT features for recognition
            rr_->template setFeatAndKeypoints<FeatureT>(v.sift_signatures_, v.sift_kp_indices_, SIFT);
        }


        //=====================Pose Estimation=======================
        typename std::map<size_t, View<PointT> >::iterator v_it;
        for (v_it = views_.begin(); v_it != views_.end(); ++v_it) {
            const View<PointT> &w = v_it->second;
            if( w.id_ ==  v.id_ )
                continue;

            std::vector<CamConnect> transforms;
            CamConnect edge;
            edge.source_id_ = v.id_;
            edge.target_id_ = w.id_;

            if(param_.scene_to_scene_) {
                edge.model_name_ = "sift_background_matching";

                boost::shared_ptr< flann::Index<DistT> > flann_index;
                convertToFLANN<FeatureT, DistT >( v.sift_signatures_, flann_index );

                std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > sift_transforms;
                estimateViewTransformationBySIFT( *w.scene_, *v.scene_, w.sift_kp_indices_.indices, v.sift_kp_indices_.indices, *w.sift_signatures_, flann_index, sift_transforms, param_.use_gc_s2s_);

                for(size_t sift_tf_id = 0; sift_tf_id < sift_transforms.size(); sift_tf_id++) {
                    edge.transformation_ = sift_transforms[sift_tf_id];
                    transforms.push_back(edge);
                }
            }

            if (param_.use_robot_pose_) {
                edge.model_name_ = "given_pose";
                Eigen::Matrix4f tf2wco_src = w.transform_to_world_co_system_;
                Eigen::Matrix4f tf2wco_trgt = v.transform_to_world_co_system_;
                edge.transformation_ = tf2wco_trgt.inverse() * tf2wco_src;
                transforms.push_back ( edge );
            }

            if( transforms.size() ) {
                size_t best_transform_id = 0;
                float lowest_edge_weight = std::numeric_limits<float>::max();

                for ( size_t trans_id = 0; trans_id < transforms.size(); trans_id++ ) {
                    CamConnect &e_tmp = transforms[trans_id];

                    try {
                        Eigen::Matrix4f icp_refined_trans;
                        e_tmp.edge_weight_ = calcEdgeWeightAndRefineTf( w.scene_, v.scene_, icp_refined_trans, e_tmp.transformation_);
                        e_tmp.transformation_ = icp_refined_trans,
                        std::cout << "Edge weight is " << e_tmp.edge_weight_ << " for edge connecting vertex " <<
                                     e_tmp.source_id_ << " and " << e_tmp.target_id_ << " by " <<  e_tmp.model_name_ ;

                        if(e_tmp.edge_weight_ < lowest_edge_weight) {
                            lowest_edge_weight = e_tmp.edge_weight_;
                            best_transform_id = trans_id;
                        }
                    }
                    catch (int e) {
                        e_tmp.edge_weight_ = std::numeric_limits<float>::max();
                        std::cerr << "Something is wrong with the SIFT based camera pose estimation. Turning it off and using the given camera poses only." << std::endl;
                        continue;
                    }
                }


                bool found = false;
                ViewD target_d;
                std::pair<vertex_iter, vertex_iter> vp;
                for (vp = vertices(gs_); vp.first != vp.second; ++vp.first) {
                    if (gs_[*vp.first] == transforms[best_transform_id].target_id_) {
                        target_d = *vp.first;
                        found = true;
                        break;
                    }
                }

                if(!found) {
                    target_d = boost::add_vertex (gs_);
                    gs_[target_d] = transforms[best_transform_id].target_id_;
                }

                ViewD src_D = boost::add_vertex (gs_);
                gs_[src_D] = transforms[best_transform_id].source_id_;
                boost::add_edge ( src_D, target_d, transforms[best_transform_id], gs_);
            }
        }

        boost::property_map<Graph, boost::edge_weight_t>::type weightmap = boost::get(boost::edge_weight, gs_);
        std::vector < EdgeD > spanning_tree;
        boost::kruskal_minimum_spanning_tree(gs_, std::back_inserter(spanning_tree));

        std::cout << "Print the edges in the MST:" << std::endl;

        for (v_it = views_.begin(); v_it != views_.end(); ++v_it)
            v_it->second.has_been_hopped_ = false;

        bool is_first_edge = true;
        std::vector<CamConnect> loose_edges;

        for (std::vector < EdgeD >::iterator ei = spanning_tree.begin(); ei != spanning_tree.end(); ++ei) {
            CamConnect e = weightmap[*ei];
            if ( !computeAbsolutePose(e, is_first_edge) )
                loose_edges.push_back(e);

            is_first_edge = false;
        }

        while(loose_edges.size()) {
            for (size_t i=0; i <loose_edges.size(); i++) {
                if ( computeAbsolutePose( loose_edges[i], is_first_edge ) )
                    loose_edges.erase(loose_edges.begin() + i);
            }
        }
    }

//    if(views_.size()>1) {
//        typename pcl::PointCloud<PointT>::Ptr registration_check (new pcl::PointCloud<PointT>);
//        typename std::map<size_t, View<PointT> >::const_iterator view_it;
//        for (view_it = views_.begin(); view_it != views_.end(); ++view_it) {   // merge feature correspondences
//            const View<PointT> &w = view_it->second;
//            typename pcl::PointCloud<PointT>::Ptr cloud_tmp (new pcl::PointCloud<PointT>);
//            pcl::transformPointCloud(*w.scene_, *cloud_tmp, w.absolute_pose_);
//            *registration_check += *cloud_tmp;
//        }
//        pcl::visualization::PCLVisualizer registration_vis("registration_check");
//        registration_vis.addPointCloud(registration_check);
//        registration_vis.spin();
//    }

    rr_->setInputCloud(v.scene_);
    rr_->setSceneNormals(v.scene_normals_);
    rr_->recognize();

    if(rr_->getSaveHypothesesParam()) {  // we have to do the correspondence grouping ourselve [Faeulhammer et al 2015, ICRA paper]
        rr_->getSavedHypotheses(v.hypotheses_);

        obj_hypotheses_.clear();

        typename pcl::PointCloud<PointT>::Ptr accum_scene (new pcl::PointCloud<PointT>(*v.scene_));
        pcl::PointCloud<pcl::Normal>::Ptr accum_normals (new pcl::PointCloud<pcl::Normal>(*v.scene_normals_));

        for (typename symHyp::const_iterator it = v.hypotheses_.begin (); it != v.hypotheses_.end (); ++it)
            obj_hypotheses_[it->first] = it->second;

        typename std::map<size_t, View<PointT> >::const_iterator view_it;
        for (view_it = views_.begin(); view_it != views_.end(); ++view_it) {   // merge feature correspondences
            const View<PointT> &w = view_it->second;
            if (w.id_ == v.id_)
                continue;

            //------ Transform keypoints and rotate normals----------
            Eigen::Matrix4f w_tf  = v.absolute_pose_.inverse() * w.absolute_pose_;
            typename pcl::PointCloud<PointT> cloud_aligned_tmp;
            pcl::transformPointCloud(*w.scene_, cloud_aligned_tmp, w_tf);
            pcl::PointCloud<pcl::Normal> normal_aligned_tmp;
            transformNormals(*w.scene_normals_, normal_aligned_tmp, w_tf);

            for (typename symHyp::const_iterator it_remote_hyp = w.hypotheses_.begin (); it_remote_hyp != w.hypotheses_.end (); ++it_remote_hyp) {
                const std::string id = it_remote_hyp->second.model_->id_;
                typename symHyp::iterator it_local_hyp = obj_hypotheses_.find(id);

                if( it_local_hyp == obj_hypotheses_.end() )   // no feature correspondences exist yet
                    obj_hypotheses_.insert(std::pair<std::string, ObjectHypothesis<PointT> >(id, it_remote_hyp->second));

                else {  // merge with existing object hypotheses
                    ObjectHypothesis<PointT> &oh_local = it_local_hyp->second;
                    const ObjectHypothesis<PointT> &oh_remote = it_remote_hyp->second;

                    size_t num_local_corr = oh_local.model_scene_corresp_->size();
                    oh_local.model_scene_corresp_->reserve( num_local_corr + oh_remote.model_scene_corresp_->size());
                    for(size_t c_id=0; c_id<oh_remote.model_scene_corresp_->size(); c_id++) {
                        const pcl::Correspondence &c_new = oh_remote.model_scene_corresp_->at(c_id);
                        const PointT &m_kp_new = oh_remote.model_->keypoints_->points[ c_new.index_query ];

                        const PointT &s_kp_new = cloud_aligned_tmp.points[ c_new.index_match ];
                        const pcl::Normal &s_kp_normal_new = normal_aligned_tmp.points[ c_new.index_match ];

                        bool drop_new_correspondence = false;

                        for(size_t cc_id=0; cc_id < num_local_corr; cc_id++) {
                            const pcl::Correspondence &c_existing = oh_local.model_scene_corresp_->at(cc_id);
                            const PointT &m_kp_existing = oh_local.model_->keypoints_->points[ c_existing.index_query ];
                            const PointT &s_kp_existing = accum_scene->points[ c_existing.index_match ];
                            const pcl::Normal &s_kp_normal_existing = accum_normals->points[ c_existing.index_match ];

                            float squaredDistModelKeypoints = pcl::squaredEuclideanDistance(m_kp_new, m_kp_existing);
                            float squaredDistSceneKeypoints = pcl::squaredEuclideanDistance(s_kp_new, s_kp_existing);

                            if( (squaredDistModelKeypoints < param_.distance_same_keypoint_) &&
                                (squaredDistSceneKeypoints < param_.distance_same_keypoint_) &&
                                (s_kp_normal_new.getNormalVector3fMap().dot(s_kp_normal_existing.getNormalVector3fMap()) > param_.same_keypoint_dot_product_) ) {

                                drop_new_correspondence = true;
                                break;
                            }
                        }

                        if (!drop_new_correspondence) {
                            oh_local.model_scene_corresp_->push_back(
                                    pcl::Correspondence(c_new.index_query, c_new.index_match + accum_scene->points.size(), c_new.distance));
                        }
                    }
                }
            }
            *accum_scene += cloud_aligned_tmp;
            *accum_normals += normal_aligned_tmp;
        }

        for (typename symHyp::iterator it = obj_hypotheses_.begin (); it != obj_hypotheses_.end (); ++it)
            it->second.model_scene_corresp_->shrink_to_fit();

//        pcl::visualization::PCLVisualizer vis_tmp;
//        pcl::visualization::PointCloudColorHandlerRGBField<PointT> handler (accum_scene);
//        vis_tmp.addPointCloud<PointT> (accum_scene, handler, "cloud wo normals");
//        vis_tmp.addPointCloudNormals<PointT,pcl::Normal>(accum_scene, accum_normals, 10);
//        vis_tmp.spin();

        scene_ = accum_scene;
        scene_normals_ = accum_normals;

        if(cg_algorithm_) {
            models_.clear();
            transforms_.clear();
            correspondenceGrouping();
            v.models_ = models_;
            v.transforms_ = transforms_;
            v.origin_view_id_.resize(models_.size());
            std::fill(v.origin_view_id_.begin(), v.origin_view_id_.end(), v.id_);
            v.model_or_plane_is_verified_.resize(models_.size());
            std::fill(v.model_or_plane_is_verified_.begin(), v.model_or_plane_is_verified_.end(), false);
        }

//        for(size_t m_id=0; m_id<transforms_.size(); m_id++) // transform hypotheses back from global coordinate system to current viewport
//                transforms_[m_id] = v.absolute_pose_.inverse() * transforms_[m_id];
    }
    else {  // correspondence grouping is done already (so we get the full models) [Faeulhammer et al 2015, MVA paper]
        v.models_ = rr_->getModels();
        v.transforms_ = rr_->getTransforms ();
        v.origin_view_id_.resize(v.models_.size());
        std::fill(v.origin_view_id_.begin(), v.origin_view_id_.end(), v.id_);
        v.model_or_plane_is_verified_.resize(v.models_.size());
        std::fill(v.model_or_plane_is_verified_.begin(), v.model_or_plane_is_verified_.end(), false);

        typename std::map<size_t, View<PointT> >::const_iterator view_it;
        for (view_it = views_.begin(); view_it != views_.end(); ++view_it) {   // add hypotheses from other views
            const View<PointT> &w = view_it->second;
            if (w.id_ == v.id_)
                continue;

            for(size_t i=0; i<w.models_.size(); i++) {
                if(w.model_or_plane_is_verified_[i]) {
                    v.models_.push_back( w.models_[i] );
                    v.transforms_.push_back( v.absolute_pose_.inverse() * w.absolute_pose_ * w.transforms_[i] );
                    v.origin_view_id_.push_back( w.origin_view_id_[i] );
                    v.model_or_plane_is_verified_.push_back( false );
                }
            }
        }

        models_ = v.models_;
        transforms_ = v.transforms_;
    }

    boost::shared_ptr<GO3D<PointT, PointT> > hv_algorithm_3d;

    if( hv_algorithm_ )
       hv_algorithm_3d = boost::dynamic_pointer_cast<GO3D<PointT, PointT>> (hv_algorithm_);

    if ( hv_algorithm_3d ) {

        NguyenNoiseModel<PointT> nm (nm_param_);
        nm.setInputCloud(v.scene_);
        nm.setInputNormals( v.scene_normals_);
        nm.compute();
        v.pt_properties_ = nm.getPointProperties();

        std::vector<typename pcl::PointCloud<PointT>::Ptr> original_clouds (views_.size());
        std::vector<pcl::PointCloud<pcl::Normal>::Ptr> normal_clouds (views_.size());
        std::vector< Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f>  > transforms_to_global  (views_.size());
        std::vector<std::vector<std::vector<float> > > pt_properties (views_.size());

        typename std::map<size_t, View<PointT> >::const_iterator v_it;
        size_t view_id = 0;
        for (v_it = views_.begin(); v_it != views_.end(); ++v_it, view_id++) {
            const View<PointT> &w = v_it->second;
            original_clouds [view_id ] = w.scene_;
            normal_clouds [view_id] = w.scene_normals_;
            transforms_to_global [view_id] = v.absolute_pose_.inverse() * w.absolute_pose_;
            pt_properties [view_id ] = w.pt_properties_;
        }

        //obtain big cloud and occlusion clouds based on noise model integration

        typename pcl::PointCloud<PointT>::Ptr octree_cloud(new pcl::PointCloud<PointT>);
        NMBasedCloudIntegration<PointT> nmIntegration (nmInt_param_);
        nmIntegration.setInputClouds(original_clouds);
        nmIntegration.setTransformations(transforms_to_global);
        nmIntegration.setInputNormals(normal_clouds);
        nmIntegration.setPointProperties(pt_properties);
        nmIntegration.compute(octree_cloud);
        pcl::PointCloud<pcl::Normal>::Ptr big_normals(new pcl::PointCloud<pcl::Normal>);
        nmIntegration.getOutputNormals(big_normals);

        std::vector<typename pcl::PointCloud<PointT>::ConstPtr> occlusion_clouds (original_clouds.size());
        for(size_t i=0; i < original_clouds.size(); i++)
            occlusion_clouds[i].reset(new pcl::PointCloud<PointT>(*original_clouds[i]));

        hv_algorithm_3d->setOcclusionClouds( occlusion_clouds );
        hv_algorithm_3d->setAbsolutePoses( transforms_to_global );

        //Instantiate HV go 3D, reimplement addModels that will reason about occlusions
        //Set occlusion cloudS!!
        //Set the absolute poses so we can go from the global coordinate system to the occlusion clouds
        //TODO: Normals might be a problem!! We need normals from the models and normals from the scene, correctly oriented!
        //right now, all normals from the scene will be oriented towards some weird 0, same for models actually
   if (views_.size() > 1 ) { // don't do this if there is only one view otherwise point cloud is not kept organized and multi-plane segmentation takes longer
            scene_ = octree_cloud;
            scene_normals_ = big_normals;
        }
        else {
            scene_ = v.scene_;
            scene_normals_ = v.scene_normals_;
        }
    }

    if ( param_.icp_iterations_ > 0 )
        poseRefinement();

    if ( hv_algorithm_ && !models_.empty() ) {
        if( !hv_algorithm_3d ) {
            scene_ = v.scene_;
            scene_normals_ = v.scene_normals_;
        }

        hypothesisVerification();
        v.model_or_plane_is_verified_ = model_or_plane_is_verified_;

        if( hv_algorithm_3d && hv_algorithm_3d->param_.visualize_cues_)
            hv_algorithm_3d->visualize();
    }

    scene_normals_.reset();

    pruneGraph();
    id_++;
}

template<typename PointT>
void
MultiviewRecognizer<PointT>::correspondenceGrouping ()
{
    for (typename symHyp::iterator it = obj_hypotheses_.begin (); it != obj_hypotheses_.end (); ++it) {
        ObjectHypothesis<PointT> &oh = it->second;
        oh.model_scene_corresp_->shrink_to_fit();

        if(oh.model_scene_corresp_->size() < 3)
            continue;

        std::vector <pcl::Correspondences> corresp_clusters;
        cg_algorithm_->setSceneCloud (scene_);
        cg_algorithm_->setInputCloud (oh.model_->keypoints_);

        if( cg_algorithm_->getRequiresNormals() )
            cg_algorithm_->setInputAndSceneNormals(oh.model_->kp_normals_, scene_normals_);

        //we need to pass the keypoints_pointcloud and the specific object hypothesis
        cg_algorithm_->setModelSceneCorrespondences (oh.model_scene_corresp_);
        cg_algorithm_->cluster (corresp_clusters);

        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > new_transforms (corresp_clusters.size());
        typename pcl::registration::TransformationEstimationSVD < PointT, PointT > t_est;

        for (size_t i = 0; i < corresp_clusters.size(); i++)
            t_est.estimateRigidTransformation (*oh.model_->keypoints_, *scene_, corresp_clusters[i], new_transforms[i]);

        if(param_.merge_close_hypotheses_) {
            std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > merged_transforms (corresp_clusters.size());
            std::vector<bool> cluster_has_been_taken(corresp_clusters.size(), false);
            const double angle_thresh_rad = param_.merge_close_hypotheses_angle_ * M_PI / 180.f ;

            size_t kept=0;
            for (size_t i = 0; i < new_transforms.size(); i++) {

                if (cluster_has_been_taken[i])
                    continue;

                cluster_has_been_taken[i] = true;
                const Eigen::Vector3f centroid1 = new_transforms[i].block<3, 1> (0, 3);
                const Eigen::Matrix3f rot1 = new_transforms[i].block<3, 3> (0, 0);

                pcl::Correspondences merged_corrs = corresp_clusters[i];

                for(size_t j=i; j < new_transforms.size(); j++) {
                    const Eigen::Vector3f centroid2 = new_transforms[j].block<3, 1> (0, 3);
                    const Eigen::Matrix3f rot2 = new_transforms[j].block<3, 3> (0, 0);
                    const Eigen::Matrix3f rot_diff = rot2 * rot1.transpose();

                    double rotx = atan2(rot_diff(2,1), rot_diff(2,2));
                    double roty = atan2(-rot_diff(2,0), sqrt(rot_diff(2,1) * rot_diff(2,1) + rot_diff(2,2) * rot_diff(2,2)));
                    double rotz = atan2(rot_diff(1,0), rot_diff(0,0));
                    double dist = (centroid1 - centroid2).norm();

                    if ( (dist < param_.merge_close_hypotheses_dist_) && (rotx < angle_thresh_rad) && (roty < angle_thresh_rad) && (rotz < angle_thresh_rad) ) {
                        merged_corrs.insert( merged_corrs.end(), corresp_clusters[j].begin(), corresp_clusters[j].end() );
                        cluster_has_been_taken[j] = true;
                    }
                }

                t_est.estimateRigidTransformation (*oh.model_->keypoints_, *scene_, merged_corrs, merged_transforms[kept]);
                kept++;
            }
            merged_transforms.resize(kept);
            new_transforms = merged_transforms;
        }

        std::cout << "Merged " << corresp_clusters.size() << " clusters into " << new_transforms.size() << " clusters. Total correspondences: " << oh.model_scene_corresp_->size () << " " << it->first << std::endl;

        //        oh.visualize(*scene_);

        size_t existing_hypotheses = models_.size();
        models_.resize( existing_hypotheses + new_transforms.size(), oh.model_  );
        transforms_.insert(transforms_.end(), new_transforms.begin(), new_transforms.end());
    }
}

}
