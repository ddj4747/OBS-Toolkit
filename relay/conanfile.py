from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class RelayDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def layout(self):
        self.folders.generators = "generators"

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build_requirements(self):
        data = self.conan_data.get("requirements", {})
        for requirement in data.get("tools", []):
            self.tool_requires(requirement)

    def requirements(self):
        data = self.conan_data.get("requirements", {})
        for requirement in data.get("common", []):
            self.requires(requirement)
