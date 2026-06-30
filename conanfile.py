import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps


class ConanApplication(ConanFile):
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)

        obs_location = self.conf.get("user.plugin:obs_location", check_type=str, default=None)
        if obs_location:
            if not os.path.isabs(obs_location):
                obs_location = os.path.normpath(os.path.join(self.source_folder, obs_location))
            tc.cache_variables["USER_PLUGIN_OBS_LOCATION"] = obs_location

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build_requirements(self):
        data = self.conan_data.get("requirements", {})

        for req in data.get("tools", []):
            self.tool_requires(req)

    def requirements(self):
        data = self.conan_data.get("requirements", {})

        for req in data.get("common", []):
            self.requires(req)