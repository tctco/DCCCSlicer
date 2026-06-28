from pathlib import Path

from setuptools import setup


try:
    from wheel.bdist_wheel import bdist_wheel
except Exception:  # pragma: no cover
    bdist_wheel = None


if bdist_wheel is not None:
    def has_vendored_runtime() -> bool:
        vendor_root = Path(__file__).resolve().parent / "src" / "dcccpy_linux_runtime" / "vendor" / "dccccore"
        return vendor_root.exists() and any(vendor_root.rglob("DCCCcore*"))


    class BinaryWheel(bdist_wheel):
        def finalize_options(self):
            super().finalize_options()
            if has_vendored_runtime():
                self.root_is_pure = False

        def get_tag(self):
            if not has_vendored_runtime():
                return super().get_tag()
            _, _, platform_tag = super().get_tag()
            return "py3", "none", platform_tag

    setup(cmdclass={"bdist_wheel": BinaryWheel})
else:
    setup()
