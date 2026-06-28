from setuptools import setup


try:
    from wheel.bdist_wheel import bdist_wheel
except Exception:  # pragma: no cover - wheel is provided by build-system.requires
    bdist_wheel = None


if bdist_wheel is not None:
    class BinaryWheel(bdist_wheel):
        """Mark wheels as platform-specific when vendored DCCCcore is present."""

        def finalize_options(self):
            super().finalize_options()
            self.root_is_pure = False

        def get_tag(self):
            _, _, platform_tag = super().get_tag()
            return "py3", "none", platform_tag

    setup(cmdclass={"bdist_wheel": BinaryWheel})
else:
    setup()
