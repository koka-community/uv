
# Example

## For whatever reason Koka doesn't like to pick up homebrew includes on MacOS, and vcpkg doesn't resolve right
koka -e examples/file-async.kk -i../std --ccincdir=/opt/homebrew/include
