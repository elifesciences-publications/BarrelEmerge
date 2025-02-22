#include_directories(${GLFW3_INCLUDE_DIR})

# James model 0. RD Karbowski with variable numbers of guidance
# molecules and TC populations and runtime setting of parameters to
# examine behaviour.
add_executable(james0 james1.cpp)
target_compile_definitions(james0 PUBLIC FLT=float COMPILE_PLOTTING)
target_link_libraries(james0 morphologica)

# Compile compute-only with floats
add_executable(james0c james1.cpp)
target_compile_definitions(james0c PUBLIC FLT=float)
target_link_libraries(james0c morphologica)

# Compile with doubles:
#add_executable(james0cd james1.cpp)
#target_compile_definitions(james0cd PUBLIC FLT=double)
#target_link_libraries(james0cd morphologica)

# Comp 2. Supp. Sect. 7.2.
add_executable(james_comp2 james1.cpp)
target_compile_definitions(james_comp2 PUBLIC FLT=float COMPILE_PLOTTING COMP2)
target_link_libraries(james_comp2 morphologica)

# Compile with doubles:
#add_executable(james_comp2d james1.cpp)
#target_compile_definitions(james_comp2d PUBLIC FLT=double COMPILE_PLOTTING COMP2)
#target_link_libraries(james_comp2d morphologica)

# Comp 2. Supp. Sect. 7.2. No plotting.
add_executable(james_comp2c james1.cpp)
target_compile_definitions(james_comp2c PUBLIC FLT=float COMP2)
target_link_libraries(james_comp2c morphologica)

# Comp 2 with linear max and graphics
add_executable(james_comp2_linmax james1.cpp)
target_compile_definitions(james_comp2_linmax PUBLIC FLT=float COMPILE_PLOTTING COMP2 LINEAR_MAX)
target_link_libraries(james_comp2_linmax morphologica)
# Comp 2 with linear max and no graphics
add_executable(james_comp2_linmaxc james1.cpp)
target_compile_definitions(james_comp2_linmaxc PUBLIC FLT=float COMP2 LINEAR_MAX)
target_link_libraries(james_comp2_linmaxc morphologica)

# Divisive normalization of a_i
add_executable(james_divnorm james1.cpp)
target_compile_definitions(james_divnorm PUBLIC FLT=float COMPILE_PLOTTING DIVNORM)
target_link_libraries(james_divnorm morphologica)

# Divisive normalization of a_i along with competition (linear)
add_executable(james_dncomp james1.cpp)
target_compile_definitions(james_dncomp PUBLIC FLT=float COMPILE_PLOTTING DNCOMP)
target_link_libraries(james_dncomp morphologica)

add_executable(james_dncompc james1.cpp)
target_compile_definitions(james_dncompc PUBLIC FLT=float DNCOMP)
target_link_libraries(james_dncompc morphologica)

# Divisive normalization of a_i along with competition of a *logistic* nature
add_executable(james_dncomp_lg james1.cpp)
target_compile_definitions(james_dncomp_lg PUBLIC FLT=float COMPILE_PLOTTING DNCOMP BRANCHCOMP_LOGISTIC_FN)
target_link_libraries(james_dncomp_lg morphologica)

# Divisive normalization of a_i along with competition of power law nature
add_executable(james_dncomp_pl james1.cpp)
target_compile_definitions(james_dncomp_pl PUBLIC FLT=float COMPILE_PLOTTING DNCOMP BRANCHCOMP_A_TO_POWER_L)
target_link_libraries(james_dncomp_pl morphologica)

# Divisive normalization of a_i along with *per-group* competition
add_executable(james_dncomp_pergroup james1.cpp)
target_compile_definitions(james_dncomp_pergroup PUBLIC FLT=float COMPILE_PLOTTING DNCOMP_PERGROUP)
target_link_libraries(james_dncomp_pergroup morphologica)

add_executable(james_dncomp_pergroupc james1.cpp)
target_compile_definitions(james_dncomp_pergroupc PUBLIC FLT=float DNCOMP_PERGROUP)
target_link_libraries(james_dncomp_pergroupc morphologica)

add_executable(dvanalyse dvanalyse.cpp)
target_include_directories(dvanalyse PUBLIC ${X11_INCLUDE_DIR})
target_compile_definitions(dvanalyse PUBLIC FLT=float COMPILE_PLOTTING)
target_link_libraries(dvanalyse morphologica morphdisplay0)

add_executable(viewguidance viewguidance.cpp)
target_include_directories(viewguidance PUBLIC ${X11_INCLUDE_DIR})
target_compile_definitions(viewguidance PUBLIC FLT=float)
target_link_libraries(viewguidance morphologica morphdisplay0)

add_executable(honda honda.cpp)
target_compile_definitions(honda PUBLIC FLT=float)
target_link_libraries(honda morphologica)

# Dual barrel fields with shared branching normalization
add_executable(jamesdual jamesdual.cpp)
target_include_directories(jamesdual PUBLIC ${X11_INCLUDE_DIR})
target_compile_definitions(jamesdual PUBLIC FLT=float COMPILE_PLOTTING)
target_link_libraries(jamesdual morphologica morphdisplay0)

# View convolution kernel
add_executable(convKernel convKernel.cpp)
target_link_libraries(convKernel morphologica)

# Small random number generator, used by scripts/gamma_noise.sh
add_executable(randomgammas randomgammas.cpp)
target_link_libraries(randomgammas morphologica)
