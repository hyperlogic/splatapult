from invoke import task
import os
import shutil


RELEASE_NAME = "splatapult-0.1-x64"
VCPKG_DIR = os.getenv("VCPKG_DIR")


@task
def clean(c):
    c.run("rm -rf build")


@task
def build(c):
    c.run("mkdir build")
    with c.cd("build"):
        c.run(
            f'cmake -DSHIPPING=ON -DCMAKE_TOOLCHAIN_FILE="{VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" ..'
        )
        c.run(f"cmake --build . --config Release")


@task
def archive(c):
    shutil.make_archive(RELEASE_NAME, "zip", "build/Release")


@task
def deploy(c):
    c.run(f"cp {RELEASE_NAME}.zip ../hyperlogic.github.io/files")
    with c.cd("../hyperlogic.github.io"):
        c.run(f"git add files/{RELEASE_NAME}.zip")
        c.run(f'git commit -m "Automated deploy of {RELEASE_NAME}"')
        c.run("git push")


@task
def all(c):
    clean(c)
    build(c)
    archive(c)
    deploy(c)
