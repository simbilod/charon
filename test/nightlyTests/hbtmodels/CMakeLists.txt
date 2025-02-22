INCLUDE(TribitsAddAdvancedTest)

###############################################################################
# test the Harmon BGN and the GaAs mobility models
SET(testName charon_mp_gaas.harmon.mob)
TRIBITS_ADD_ADVANCED_TEST(
  ${testName}
  CATEGORIES NIGHTLY
  OVERALL_WORKING_DIRECTORY TEST_NAME

  TEST_0 COPY_FILES_TO_TEST_DIR
    gaas.harmon.mob.inp
    gaas.0.8V.exo.4.0
    gaas.0.8V.exo.4.1
    gaas.0.8V.exo.4.2
    gaas.0.8V.exo.4.3
    GaAs_LowField_EMob.txt
    GaAs_LowField_HMob.txt

  TEST_1 CMND ${PACKAGE_BINARY_DIR}/charonInterpreter
    ARGS -i gaas.harmon.mob.inp

  TEST_2 EXEC charon_mp NOEXEPREFIX DIRECTORY ${PACKAGE_BINARY_DIR}
    ARGS  --i=gaas.harmon.mob.inp.xml --current
    NUM_MPI_PROCS 4         
    PASS_REGULAR_EXPRESSION "Charon run completed."
    OUTPUT_FILE gaas.harmon.mob.output

  # test that the current is correct
  TEST_3 CMND python3
    ARGS ${CMAKE_SOURCE_DIR}/scripts/utils/current_verification.py
         --filename       gaas.harmon.mob.output
         --anodeCurrent   -1.92189078e-06
         --cathodeCurrent 1.92189075e-06
         --tolerance      2.0e-7
    PASS_REGULAR_EXPRESSION "Test Passed"

  ADDED_TEST_NAME_OUT ${testName}_CREATED
  )
IF (${testName}_CREATED)
  SET_PROPERTY(TEST Charon_charon_mp_gaas.harmon.mob  PROPERTY LABELS
    gaas.harmon.mob nightly)
ENDIF()
