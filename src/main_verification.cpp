#include <iostream>

#include <assert.h>
#include <boost/lexical_cast.hpp>

#include "Mesh.h"
#include "Model.h"
#include "PnPProblem.h"
#include "ModelRegistration.h"
#include "Utils.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>


  /*
   * Set up the images paths
   */
  std::string img_path = "../Data/resized_IMG_3875.JPG";
  std::string img_verification_path = "../Data/resized_IMG_3872.JPG";
  std::string ply_read_path = "../Data/box.ply";
  std::string yml_read_path = "../Data/box.yml";

  // Boolean the know if the registration it's done
  bool end_registration = false;

  // Setup the points to register in the image
  // In the order of the *.ply file and starting at 1
  int n = 7;
  int pts[] = {1, 2, 3, 5, 6, 7, 8};

  /*
   * Set up the intrinsic camera parameters: CANON
   */
  double f = 55;
  double sx = 22.3, sy = 14.9;
  double width = 718, height = 480;
  double params_CANON[] = { width*f/sx,   // fx
                            height*f/sy,  // fy
                            width/2,      // cx
                            height/2};    // cy


 /*
   * Set up the intrinsic camera parameters: UVC WEBCAM
   */
  double f2 = 43;
  double params_CANON2[] = { width*f2/sx,   // fx
                             height*f2/sy,  // fy
                             width/2,       // cx
                             height/2};     // cy

  /*
   * Set up some basic colors
   */
  cv::Scalar red(0, 0, 255);
  cv::Scalar green(0,255,0);
  cv::Scalar blue(255,0,0);
  cv::Scalar yellow(0,255,255);

  /*
   * CREATE MODEL REGISTRATION OBJECT
   * CREATE OBJECT MESH
   * CREATE OBJECT MODEL
   * CREATE PNP OBJECT
   */
  Mesh mesh;
  ModelRegistration registration;
  PnPProblem pnp_verification(params_CANON);
  PnPProblem pnp_verification_gt(params_CANON2);


// Mouse events for model registration
static void onMouseModelVerification( int event, int x, int y, int, void* )
{
  if  ( event == cv::EVENT_LBUTTONUP )
  {
      int n_regist = registration.getNumRegist();
      int n_vertex = pts[n_regist];

      cv::Point2f point_2d = cv::Point2f(x,y);
      cv::Point3f point_3d = mesh.getVertex(n_vertex-1);

      bool is_registrable = registration.is_registrable();
      if (is_registrable)
      {
        registration.registerPoint(point_2d, point_3d);
        if( registration.getNumRegist() == registration.getNumMax() ) end_registration = true;
      }
  }
}


/*
 *   MAIN PROGRAM
 *
 */

int main(int, char**)
{

  std::cout << "!!!Hello Verification!!!" << std::endl; // prints !!!Hello World!!!

  // load a mesh given the *.ply file path
  mesh.load(ply_read_path);

  // load the 3D textured object model
  Model model;
  model.load(yml_read_path);


  /*
  * GROUND TRUTH SECOND IMAGE
  *
  */

  cv::Mat img_in, img_in2, img_vis;

  // Setup for new registration
  registration.setNumMax(n);

  // Create & Open Window
  cv::namedWindow("MODEL GROUND TRUTH", cv::WINDOW_KEEPRATIO);

  // Set up the mouse events
  cv::setMouseCallback("MODEL GROUND TRUTH", onMouseModelVerification, 0 );

  // Open the image to register
  img_in = cv::imread(img_path, cv::IMREAD_COLOR);
  img_in2 = cv::imread(img_verification_path, cv::IMREAD_COLOR);

  if (!img_in.data || !img_in2.data)
  {
    std::cout << "Could not open or find the image" << std::endl;
    return -1;
  }

  std::cout << "Click the box corners ..." << std::endl;
  std::cout << "Waiting ..." << std::endl;

  // Loop until all the points are registered
  while ( cv::waitKey(30) < 0 )
  {
    // Refresh debug image
    img_vis = img_in2.clone();

    // Current registered points
    std::vector<cv::Point2f> list_points2d = registration.get_points2d();
    std::vector<cv::Point3f> list_points3d = registration.get_points3d();

    // Draw current registered points
    drawPoints(img_vis, list_points2d, list_points3d, red);

    // If the registration is not finished, draw which 3D point we have to register.
    // If the registration is finished, breaks the loop.
    if (!end_registration)
    {
      // Draw debug text
      int n_regist = registration.getNumRegist();
      int n_vertex = pts[n_regist];
      cv::Point3f current_poin3d = mesh.getVertex(n_vertex-1);

      drawQuestion(img_vis, current_poin3d, green);
      drawCounter(img_vis, registration.getNumRegist(), registration.getNumMax(), red);
    }
    else
    {
      // Draw debug text
      drawText(img_vis, "GROUND TRUTH", green);
      drawCounter(img_vis, registration.getNumRegist(), registration.getNumMax(), green);
      break;
    }

    // Show the image
    cv::imshow("MODEL GROUND TRUTH", img_vis);
  }

  // The list of registered points
  std::vector<cv::Point2f> list_points2d = registration.get_points2d();
  std::vector<cv::Point3f> list_points3d = registration.get_points3d();

  // Estimate pose given the registered points
  bool is_correspondence = pnp_verification_gt.estimatePose(list_points3d, list_points2d, cv::ITERATIVE);

  // Compute and draw all mesh object 2D points
  std::vector<cv::Point2f> pts_2d_ground_truth = pnp_verification_gt.verify_points(&mesh);
  draw2DPoints(img_vis, pts_2d_ground_truth, green);

  // Draw the ground truth mesh
  drawObjectMesh(img_vis, &mesh, &pnp_verification_gt, blue);

  // Show the image
  cv::imshow("MODEL GROUND TRUTH", img_vis);

  // Show image until ESC pressed
  cv::waitKey(0);


  /*
   * PNP VERIFICATION
   *
   */


  // Get the MODEL INFO
  std::vector<cv::Point2f> list_points2d_model = model.get_points2d_in();
  std::vector<cv::Point3f> list_points3d_model = model.get_points3d();
  cv::Mat descriptors_model = model.get_descriptors();

  // CREATE MATCHER
  int normType = cv::NORM_HAMMING;
  bool crossCheck = false;
  cv::BFMatcher matcher(normType, crossCheck);

  // Add the descriptor to the matcher and train
  matcher.add(descriptors_model);
  matcher.train();

  //-- Step 1 & 2: Calculate keypoints and descriptors
  std::vector<cv::KeyPoint> keypoints_model, keypoints_scene;
  cv::Mat descriptors_scene;
  computeKeyPoints(img_in, keypoints_model, descriptors_model);
  computeKeyPoints(img_in2, keypoints_scene, descriptors_scene);

  // -- Step 3: Match the found keypoints
  std::vector<std::vector<cv::DMatch> > matches;
  matcher.knnMatch(descriptors_scene, matches, 2); // Find two nearest matches

  // -- Step 4: Ratio test
  std::vector<cv::DMatch> good_matches;
  for (unsigned int match_index = 0; match_index < matches.size(); ++match_index)
  {
   const float ratio = 0.8; // As in Lowe's paper; can be tuned
   if (matches[match_index][0].distance < ratio * matches[match_index][1].distance)
   {
       good_matches.push_back(matches[match_index][0]);
   }
  }

  // -- Step 5: Find out the 2D/3D correspondences
  std::vector<cv::Point3f> list_points3d_model_match;
  std::vector<cv::Point2f> list_points2d_scene_match;
  for(unsigned int match_index = 0; match_index < good_matches.size(); ++match_index)
  {
   cv::Point3f point3d_model = list_points3d_model[ good_matches[match_index].trainIdx ];
   cv::Point2f point2d_scene = keypoints_scene[ good_matches[match_index].queryIdx ].pt;
   list_points3d_model_match.push_back(point3d_model);
   list_points2d_scene_match.push_back(point2d_scene);
  }

  // -- Step 6: Pose estimation using RANSAC approach
  cv::Mat inliers;
  pnp_verification.estimatePoseRANSAC( list_points3d_model_match, list_points2d_scene_match, cv::EPNP, inliers );

  // -- Step 7: Catch the inliers keypoints
  std::vector<cv::DMatch> matches_inliers;
  std::vector<cv::Point2f> list_points2d_inliers;
  for(int inliers_index = 0; inliers_index < inliers.rows; ++inliers_index)
  {
   int n = inliers.at<int>(inliers_index);
   cv::Point2f point2d = list_points2d_scene_match[n];
   list_points2d_inliers.push_back(point2d);

   unsigned int match_index = 0;
   bool is_equal = equal_point( point2d, keypoints_scene[good_matches[match_index].queryIdx].pt );
   while ( !is_equal && match_index < good_matches.size() )
   {
     match_index++;
     is_equal = equal_point( point2d, keypoints_scene[good_matches[match_index].queryIdx].pt );
   }
   matches_inliers.push_back(good_matches[match_index]);
  }

  // MESH VERIFICATION & DRAW POSE
  // Compute all the 2D points of the mesh to verify the algorithm and draw it
  drawObjectMesh(img_in2, &mesh, &pnp_verification_gt, green);
  drawObjectMesh(img_in2, &mesh, &pnp_verification, yellow);

  // Draw pose
  double l = 5;
  std::vector<cv::Point2f> pose_points2d;
  pose_points2d.push_back(pnp_verification.backproject3DPoint(cv::Point3f(0,0,0)));
  pose_points2d.push_back(pnp_verification.backproject3DPoint(cv::Point3f(l,0,0)));
  pose_points2d.push_back(pnp_verification.backproject3DPoint(cv::Point3f(0,l,0)));
  pose_points2d.push_back(pnp_verification.backproject3DPoint(cv::Point3f(0,0,l)));
  draw3DCoordinateAxes(img_in2, pose_points2d);

  // -- Step X: Draw correspondences

  // Switched the order due to a opencv bug
  cv::drawMatches(img_in2, keypoints_scene, img_in, keypoints_model, matches_inliers, img_vis, red, blue);

  // -- Step X: Draw some text for debugging purpose

  // Draw some debug text
  drawText(img_vis, "GROUND TRUTH", green);
  drawText2(img_vis, "ESTIMATION", yellow);

  cv::imshow("MODEL GROUND TRUTH", img_vis);


  // PNP ERRORS:
  // Calculation of the rotation and translation error

  cv::Mat t_true = pnp_verification_gt.get_t_matrix();
  cv::Mat t = pnp_verification.get_t_matrix();

  cv::Mat R_true = pnp_verification_gt.get_R_matrix();
  cv::Mat R = pnp_verification.get_R_matrix();

  double error_trans = get_translation_error(t_true, t);
  double error_rot = get_rotation_error(R_true, R)*180/CV_PI;

  std::cout << "Translation error: " << error_trans << std::endl;
  std::cout << "Rotation error: " << error_rot << std::endl;

  // Show image until ESC pressed
  cv::waitKey(0);

  // Close and Destroy Window
  cv::destroyWindow("MODEL GROUND TRUTH");

}