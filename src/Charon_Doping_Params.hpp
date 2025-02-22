
#ifndef CHARON_DOPING_PARSER
#define CHARON_DOPING_PARSER

#include "Teuchos_ParameterList.hpp"
#include <vector>

namespace charon {
  class uniformDopingParams
  {
    public:
        uniformDopingParams() :
             dopType("None"),
             dopVal(0.0),
             xmin(-1e100),
             xmax( 1e100),
             ymin(-1e100),
             ymax( 1e100),
             zmin(-1e100),
             zmax( 1e100),
             sweepMe(false)
        {}

        void parseUniform (const Teuchos::ParameterList& plist);
        std::string dopType;
        double dopVal;
        double initialDopVal, finalDopVal;
        double xmin;
        double xmax;
        double ymin;
        double ymax;
        double zmin;
        double zmax;
        bool sweepMe;
  };

  class gaussianDopingParams
  {
    void testcoord (const std::string axis, const Teuchos::ParameterList& plist,
                    std::string& dir, double& min, double& max,
                    double& loc, double& width, bool& checkAxis);
    public:
      gaussianDopingParams(): y_loc(0.0), y_width(0.0), y_min(0.0), y_max(0.0),
                              z_loc(0.0), z_width(0.0), z_min(0.0), z_max(0.0)
    {;};

      ~gaussianDopingParams(){;}

      void parseGaussian (const Teuchos::ParameterList& plist, const int num_dim);


      std::string dopType;
      double maxVal;
      double minVal;
      // x direction
      std::string x_dir;
      double x_loc;
      double x_width;
      double x_min;
      double x_max;
      bool x_checkAxis;
      // y direction
      std::string y_dir;
      double y_loc;
      double y_width;
      double y_min;
      double y_max;
      bool y_checkAxis;
      // z direction
      std::string z_dir;
      double z_loc;
      double z_width;
      double z_min;
      double z_max;
      bool z_checkAxis;
  };

  class linearDopingParams
  {
    void testcoord (const std::string axis, const Teuchos::ParameterList& plist,
                    double& min, double& max, bool& checkAxis);
    public:
      linearDopingParams(): y_min(0.0), y_max(0.0), z_min(0.0), z_max(0.0)
    {;};

      ~linearDopingParams(){;}

      void parseLinear (const Teuchos::ParameterList& plist, const int num_dim);

      std::string dopType;
      double startVal;
      double endVal;
      // x direction
      double x_min;
      double x_max;
      bool x_checkAxis;
      // y direction
      double y_min;
      double y_max;
      bool y_checkAxis;
      // z direction
      double z_min;
      double z_max;
      bool z_checkAxis;
  };

  class erfcDopingParams
  {
    void testcoord (const std::string axis, const Teuchos::ParameterList& plist,
                    std::string& dir, double& min, double& max,
                    double& loc, double& width, bool& checkAxis);
    public:
      erfcDopingParams(): y_loc(0.0), y_width(0.0), y_min(0.0), y_max(0.0),
                              z_loc(0.0), z_width(0.0), z_min(0.0), z_max(0.0)
    {;};

      ~erfcDopingParams(){;}

      void parseErfc (const Teuchos::ParameterList& plist, const int num_dim);


      std::string dopType;
      double maxVal;
      double minVal;
      // x direction
      std::string x_dir;
      double x_loc;
      double x_width;
      double x_min;
      double x_max;
      bool x_checkAxis;
      // y direction
      std::string y_dir;
      double y_loc;
      double y_width;
      double y_min;
      double y_max;
      bool y_checkAxis;
      // z direction
      std::string z_dir;
      double z_loc;
      double z_width;
      double z_min;
      double z_max;
      bool z_checkAxis;
  };

  class mgaussDopingParams
  {
    void testcoord (const std::string axis, const Teuchos::ParameterList& plist,
                    std::string& dir, double& min, double& max,
                    bool& checkErfc, double& width, bool& checkAxis);
    public:
      mgaussDopingParams(): y_width(0.0), y_min(0.0), y_max(0.0),
                              z_width(0.0), z_min(0.0), z_max(0.0)
    {;};

      ~mgaussDopingParams(){;}

      void parseMGauss (const Teuchos::ParameterList& plist, const int num_dim);


      std::string dopType;
      double maxVal;
      double minVal;
      // x direction
      std::string x_dir;
      double x_width;
      double x_min;
      double x_max;
      bool x_checkErfc;
      bool x_checkAxis;
      // y direction
      std::string y_dir;
      double y_width;
      double y_min;
      double y_max;
      bool y_checkErfc;
      bool y_checkAxis;
      // z direction
      std::string z_dir;
      double z_width;
      double z_min;
      double z_max;
      bool z_checkErfc;
      bool z_checkAxis;
  };

  class haloDopingParams
  {
    void testcoord (const std::string axis, const Teuchos::ParameterList& plist);
    public:
    haloDopingParams(): x_center(0.0),y_center(0.0), z_center(0.0),r1(0.0), r2(0.0), rotation(0.0)
    {;};

    ~haloDopingParams(){;}

    void parseHalo (const Teuchos::ParameterList& plist, const int num_dim);


    std::string dopType,distributionType;
    double dopingVal;
    double minDopingVal;
    double width;

    // x direction
    double x_center;
    bool x_checkAxis;

    // y direction
    double y_center;
    bool y_checkAxis;

    // z direction
    double z_center;
    bool z_checkAxis;

    //ellipsoid axes
    double r1,r2;
    double rotation;


  };

}  //namespace charon

#endif //CHARON_DOPING_PARSER
