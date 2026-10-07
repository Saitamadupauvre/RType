set(VCPKG_BUILD_TYPE release)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO lucaspujol/clap
    REF "v${VERSION}"
    SHA512 04eaa8573668adefbbd845805f209ecfa95c8db05ad808e4d6b426650f2bd64698ceb0d87877da5506fe26df1f43fd693f78ab07af20553c023ad34df8cdb3ab
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS -DCLAP_BUILD_TESTS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/clap)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
