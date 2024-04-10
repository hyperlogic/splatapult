from invoke import task
import os
import shutil


RELEASE_NAME = "splatapult-0.1-x64"

@task
def clean(c):
    c.run("rm -rf vcpkg_installed")
    c.run("rm -rf build")

def build_with_config(c, config):
    c.run("mkdir build")

    with c.cd("build"):
        c.run('cmake -DSHIPPING=ON -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake" ..')
        c.run(f"cmake --build . --config={config}")

@task
def build(c):
    build_with_config(c, "Release")

@task
def build_debug(c):
    build_with_config(c, "Debug")

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
