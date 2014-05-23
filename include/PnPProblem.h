/*
 * PnPProblem.h
 *
 *  Created on: Mar 28, 2014
 *      Author: Edgar Riba
 */

#ifndef PNPPROBLEM_H_
#define PNPPROBLEM_H_

#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include "Mesh.h"
#include "ModelRegistration.h"

class PnPProblem
{

public:
  explicit PnPProblem(const double param[]);  // custom constructor
  virtual ~PnPProblem();

  bool estimatePose(const std::vector<cv::Point2f> &list_points2d, const std::vector<cv::Point3f> &list_points3d, int flags);
  bool backproject2DPoint(const Mesh *mesh, const cv::Point2f &point2d, cv::Point3f &point3d);
  bool intersect_MollerTrumbore(Ray &R, Triangle &T, double *out);
  std::vector<cv::Point2f> verify_points(Mesh *mesh);
  cv::Point2f backproject3DPoint(const cv::Point3f &point3d);

private:
  /** The calibration matrix */
  cv::Mat _A_matrix;
  /** The computed rotation matrix */
  cv::Mat _R_matrix;
  /** The computed translation matrix */
  cv::Mat _t_matrix;
  /** The computed projection matrix */
  cv::Mat _P_matrix;
};

#endif /* PNPPROBLEM_H_ */
