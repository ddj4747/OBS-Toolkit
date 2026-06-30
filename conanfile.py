from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps

class ConanApplication(ConanFile):
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        pass

    def build_requirements(self):
        data = self.conan_data.get("requirements", {})

        for req in data.get("tools", []):
            self.tool_requires(req)

    def requirements(self):
        data = self.conan_data.get("requirements", {})

        for req in data.get("common", []):
            self.requires(req)