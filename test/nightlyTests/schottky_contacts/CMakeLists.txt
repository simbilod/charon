INCLUDE(TribitsAddAdvancedTest)

####################################################
# Schottky n-type test 
SET(testName charon_mp_Schottky_n)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    n_diode_msh.jou  
    n_diode.dd.exo.4.0
    n_diode.dd.exo.4.1
    n_diode.dd.exo.4.2
    n_diode.dd.exo.4.3
    n_diode.dd.forward.inp
    n_diode.dd.forward.gold.exo
    n_diode.dd.forward.exodiff
    n_diode.dd.reverse.inp
    n_diode.dd.reverse.gold.exo
    n_diode.dd.reverse.exodiff

  TEST_1 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i n_diode.dd.forward.inp

  TEST_2 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.forward.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_3 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "n_diode.dd.forward.exo.4."
    NUM_MPI_PROCS 1

  TEST_4 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         n_diode.dd.forward.exodiff
         n_diode.dd.forward.exo
         n_diode.dd.forward.gold.exo
    NUM_MPI_PROCS 1

  TEST_5 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i n_diode.dd.reverse.inp

  TEST_6 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.reverse.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_7 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "n_diode.dd.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_8 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         n_diode.dd.reverse.exodiff
         n_diode.dd.reverse.exo
         n_diode.dd.reverse.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_n PROPERTY LABELS
  Schottky_n nightly)
ENDIF()

####################################################
# Schottky p-type test 
SET(testName charon_mp_Schottky_p)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    p_diode_msh.jou  
    p_diode.dd.exo.4.0
    p_diode.dd.exo.4.1
    p_diode.dd.exo.4.2
    p_diode.dd.exo.4.3
    p_diode.dd.forward.inp
    p_diode.dd.forward.gold.exo
    p_diode.dd.forward.exodiff
    p_diode.dd.reverse.inp
    p_diode.dd.reverse.gold.exo
    p_diode.dd.reverse.exodiff

  TEST_1 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i p_diode.dd.forward.inp

  TEST_2 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.forward.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_3 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "p_diode.dd.forward.exo.4."
    NUM_MPI_PROCS 1

  TEST_4 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         p_diode.dd.forward.exodiff
         p_diode.dd.forward.exo
         p_diode.dd.forward.gold.exo
    NUM_MPI_PROCS 1

  TEST_5 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i p_diode.dd.reverse.inp

  TEST_6 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.reverse.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_7 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "p_diode.dd.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_8 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         p_diode.dd.reverse.exodiff
         p_diode.dd.reverse.exo
         p_diode.dd.reverse.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_p PROPERTY LABELS
  Schottky_p nightly)
ENDIF()

####################################################
# Schottky n-type BL test 
SET(testName charon_mp_Schottky_n_BL)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    n_diode.nlp.exo.4.0
    n_diode.nlp.exo.4.1
    n_diode.nlp.exo.4.2
    n_diode.nlp.exo.4.3
    n_diode.dd.BL.inp
    n_diode.dd.BL.reverse.inp
    n_diode.dd.BL.reverse.gold.exo
    n_diode.dd.BL.reverse.exodiff

  TEST_1 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i n_diode.dd.BL.inp

  TEST_2 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i n_diode.dd.BL.reverse.inp

  TEST_3 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.BL.inp.xml
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  TEST_4 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.BL.reverse.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_5 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "n_diode.dd.BL.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_6 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         n_diode.dd.BL.reverse.exodiff
         n_diode.dd.BL.reverse.exo
         n_diode.dd.BL.reverse.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_n_BL PROPERTY LABELS
  Schottky_n_BL nightly)
ENDIF()


####################################################
# Schottky p-type BL test 
SET(testName charon_mp_Schottky_p_BL)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    p_diode.nlp.exo.4.0
    p_diode.nlp.exo.4.1
    p_diode.nlp.exo.4.2
    p_diode.nlp.exo.4.3
    p_diode.dd.BL.inp
    p_diode.dd.BL.reverse.inp
    p_diode.dd.BL.reverse.gold.exo
    p_diode.dd.BL.reverse.exodiff

  TEST_1 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i p_diode.dd.BL.inp

  TEST_2 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter 
    ARGS -i p_diode.dd.BL.reverse.inp

  TEST_3 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.BL.inp.xml
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  TEST_4 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.BL.reverse.inp.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_5 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "p_diode.dd.BL.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_6 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         p_diode.dd.BL.reverse.exodiff
         p_diode.dd.BL.reverse.exo
         p_diode.dd.BL.reverse.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_p_BL PROPERTY LABELS
  Schottky_p_BL nightly)
ENDIF()

####################################################
# Schottky n-type tunneling test 
SET(testName charon_mp_Schottky_n_tun)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    n_diode.nlp.exo.4.0
    n_diode.nlp.exo.4.1
    n_diode.nlp.exo.4.2
    n_diode.nlp.exo.4.3
    n_diode.dd.tun.xml
    n_diode.dd.tun.reverse.xml
    n_diode.dd.tun.reverse.gold.exo
    n_diode.dd.tun.reverse.exodiff
    n_diode.dd.tun.forward.xml
    n_diode.dd.tun.forward.gold.exo
    n_diode.dd.tun.forward.exodiff
    n_diode.dd.tun.reverse.xml
    n_diode.dd.tun.reverse.gold.exo
    n_diode.dd.tun.reverse.exodiff

  TEST_1 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.tun.xml
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  TEST_2 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.tun.reverse.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_3 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "n_diode.dd.tun.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_4 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         n_diode.dd.tun.reverse.exodiff
         n_diode.dd.tun.reverse.exo
         n_diode.dd.tun.reverse.gold.exo
    NUM_MPI_PROCS 1

  TEST_5 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=n_diode.dd.tun.forward.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_6 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "n_diode.dd.tun.forward.exo.4."
    NUM_MPI_PROCS 1

  TEST_7 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         n_diode.dd.tun.forward.exodiff
         n_diode.dd.tun.forward.exo
         n_diode.dd.tun.forward.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_n_tun PROPERTY LABELS
  Schottky_n_tun nightly)
ENDIF()


####################################################
# Schottky p-type tunneling test 
SET(testName charon_mp_Schottky_p_tun)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    p_diode.nlp.exo.4.0
    p_diode.nlp.exo.4.1
    p_diode.nlp.exo.4.2
    p_diode.nlp.exo.4.3
    p_diode.dd.tun.xml
    p_diode.dd.tun.reverse.xml
    p_diode.dd.tun.reverse.gold.exo
    p_diode.dd.tun.reverse.exodiff
    p_diode.dd.tun.forward.xml
    p_diode.dd.tun.forward.gold.exo
    p_diode.dd.tun.forward.exodiff
    p_diode.dd.tun.reverse.xml
    p_diode.dd.tun.reverse.gold.exo
    p_diode.dd.tun.reverse.exodiff

  TEST_1 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.tun.xml
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  TEST_2 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.tun.reverse.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_3 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "p_diode.dd.tun.reverse.exo.4."
    NUM_MPI_PROCS 1

  TEST_4 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         p_diode.dd.tun.reverse.exodiff
         p_diode.dd.tun.reverse.exo
         p_diode.dd.tun.reverse.gold.exo
    NUM_MPI_PROCS 1

  TEST_5 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS --i=p_diode.dd.tun.forward.xml --current
    NUM_MPI_PROCS 4
    PASS_REGULAR_EXPRESSION "Charon run completed."

  # test the correctness of the exodus output
  TEST_6 EXEC ${SEACAS_BINARY_DIR}/applications/epu/epu NOEXEPREFIX NOEXESUFFIX
    ARGS  "-auto" "p_diode.dd.tun.forward.exo.4."
    NUM_MPI_PROCS 1

  TEST_7 EXEC ${SEACAS_BINARY_DIR}/applications/exodiff/exodiff NOEXEPREFIX
    NOEXESUFFIX
    ARGS -f
         p_diode.dd.tun.forward.exodiff
         p_diode.dd.tun.forward.exo
         p_diode.dd.tun.forward.gold.exo
    NUM_MPI_PROCS 1

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_Schottky_p_tun PROPERTY LABELS
  Schottky_p_tun nightly)
ENDIF()
