To inlcude Masala into your OpenWRT image or to create
an .ipk package (equivalent to Debians .deb files), you
have to building an OpenWRT image.
These steps were tested for OpenWRT-"Attitude Adjustment":

<pre>
svn co svn://svn.openwrt.org/openwrt/trunk openwrt
cd openwrt

./scripts/feeds update -a
./scripts/feeds install -a

https://github.com/mwarning/masala.git
cp -rf masala/openwrt/masala package/
rm -rf masala/

make defconfig
make menuconfig
</pre>

At this point select the appropiate "Target System" and "Target Profile"
depending on what target chipset/router you want to build for.
To get an *.ipk file you also need to select "Build the OpenWrt SDK"

Now compile/build everything:

<pre>
make
</pre>

The images and all *.ipk packages are now inside the bin/ folder.
You can install the Masala .ipk using "opkg install <ipkg-file>" on the router.

For details please consult the OpenWRT documentation.
