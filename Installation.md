There are several ways of installing the drivers or the whole packet on your machine. You either can download a file tree or a snapshot of a driver to somewhere, unpack it and do the makes inside there accordingly. Alternatively, you could use the subversion mechanism; this makes it really simple to install working directories on your machine, and keep the drivers up to date. Another method (but this is not implemented yet) would use installation packets like rpm or the debian equivalent if you are only interested in the compiled versions of the driver for your particular system.



# Prerequisits #
Perhaps this is a bit of a stupid remark on this platform, but you need to make sure that you have installed a few packets on your machine:
  * The subversion packet management system
  * A set of development tools. Specifically, ensure that you have
    * **gcc** as a compiler. Perhaps it works with others as well, but I have not tried
    * the **gnu make** utility
    * The **kernel sources** or at least the include files which allow you to compile kernel modules

# Installing only a single driver via svn #
If you only want to install a single driver (e.g. `dt302`) into your `driverdirectory`, follow these steps:
  1. cd into the `driverdirectory`
  1. issue the command<br><code> svn checkout https://linuxmeasurement.googlecode.com/svn/trunk/dt302</code></li></ul>

If you later want to update the driver, cd into the directory <code>dt302</code> (for the above example) and issue the command <code>svn update</code> - then, don't forget to issue the <code>make</code> instructions.<br>
<br>
<br>
<h1>Installation of all packets via svn #
Assuming that you want your drivers located in a subdirectory named `programdirectory` in the directory `homedirectory`, then you can install all the drivers with the following steps:
  1. cd into `homedirectory`
  1. get all code with the following command:<br><code>svn checkout https://linuxmeasurement.googlecode.com/svn/trunk/ programdirectory</code>
<ol><li>cd into the individual subdirectories and follow the make instructions there.</li></ol>

You should be able to add code and other directories as well at your heart's content. If you want to update all drivers at once from the repository, go into the program directory and issue the command <code>svn update</code> . All the repository and status information is kept in the hidden directories <code>.svn</code>. Specifically, an update will not affect any other directories and files you have created.<br>
<br>
<br>
<h1>Migration from an existing driver directory to one with svn control</h1>
If there is already a directory on your machine with roughly the necessary directory structure as for the drivers here, it would be nice to have a way to migrate them to a svn controlled way without disruption of the current work.<br>
<br>
At the moment, I am not sure if there exists already a clean svn command for this, but it looks like the following recipie works without loosing other stuff which is in this directory.  The example assumes you have a directory <code>dt302</code> installed in the <code>programs</code> directory, which contains not only the driver but also some other stuff, e.g. a directory with measurement data files:<br>
<ol><li>cd into the directory which hosts the driver directory:<br><code>cd programs</code>
</li><li>Rename the currently existing driver directory into something temporary:<br><code>mv dt302 dt302.tmp</code><br>This protects the whole directory from being tampered with.<br>
</li><li>Generate a new driver directory which contains all the svn framework:<br><code>svn checkout https://linuxmeasurement.googlecode.com/svn/trunk/dt302</code><br>You should have a virgin copy of the driver directory now.<br>
</li><li>cd into the new directory and generate the driver files:<br><code>cd dt320 ; make</code><br>You want to do this at this point because otherwise you get all the old compiled drivers into the new directory.<br>
</li><li>Copy the stuff which does not exist in the new directory over from the old directory (assuming that you are in the new directory already):<br><code>cp -purv ../dt302.tmp/* ./</code><br>This should copy over all the stuff which did exist in the old directory and for which there is no correspondence in the drive directory from the repository.<br>
</li><li>You need to inspect the log of the copy command with some care to find out if there are files copied over you actually don't want in the new directory, like makefiles which have a slightly different name. Remove such files in the new directory to avoid some problems.<br>
</li><li>Once you are really, really, really sure that you have everything copied over what you need, you can discard the temporary directory (or move it to a place where it is out of the way).<br>
</li><li>Depending on which driver you use, you may have to reboot the machine or, if you want to change the driver on the fly, remove the corresponding module out of the operating kernel with <code>rmmod</code> and insert the new one with <code>insmod</code>.