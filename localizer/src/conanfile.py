# conanfile.py
from conan import ConanFile


class AppConan(ConanFile):
    name = "centiloidcalculator"
    version = "2.4.0"
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        # your direct deps (use exact versions or ranges as you prefer)
        self.requires("argparse/3.2")
        self.requires("itk/5.3.0")
        self.requires("onnxruntime/1.18.1")
        self.requires("tomlplusplus/3.4.0")
        self.requires("onetbb/2021.12.0")

        # ---- conflict resolution: choose one Eigen for the whole graph ----
        # If the graph shows ORT wants 3.4.0, prefer:
        self.requires("eigen/3.4.0")
        # If you decide to *force* a direct one instead (when you also require eigen directly):
        # self.requires("eigen/3.4.0", force=True)
