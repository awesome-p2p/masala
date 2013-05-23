To inlcude Masala into your OpenWRT image or to create
an .ipk package (equivalent to Debians .deb files),
you have to build an OpenWRT image.
These steps were tested using OpenWRT-"Attitude Adjustment":

<pre>
svn co svn://svn.openwrt.org/openwrt/branches/attitude_adjustment openwrt
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
You can install the Masala .ipk using "opkg install &lt;ipkg-file&gt;" on the router.

For details please check the OpenWRT documentation.

#### Note for developers:

You might want to your own source location and not the remote respository.
To do this create a symbolic link into the masala package folder:

<pre>
ln -s /my/own/project/folder/masala/.git openwrt/package/masala/git-src
</pre>
