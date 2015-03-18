# Disclaimer #
At the point of uploading code to this repository, we use different drivers under different distros/kernel version, and we often don't make an effort to ensure all drivers work in all combinations. This page should give an idea of where which driver is confirmed to work.

Keep in mind that this list is very incomplete, since one needs to verify if the uploaded code is the same as what we use on various machines. But chances are that the code works on reasonably similar kernels. Another reason for this list to be incomplete is that I only visit it when fixing something.

# Confirmed list #

| **Driver** | **Confirmed to work in the configuration** | **Known limitations** |
|:-----------|:-------------------------------------------|:----------------------|
| dt302 | kernel 2.6.25-0.2-pae on i686, OpenSuSE 11.0<br>kernel 2.6.27.25-0.1 SMP on x86_64, Opensuse 11.1 <table><thead><th> many progs fixed with rev 31 </th></thead><tbody>
<tr><td> dt340 </td><td> kernel 2.6.25-0.2-pae on i686, OpenSuSE 11.0<br>kernel 2.6.27.25-0.1 SMP on x86_64, Opensuse 11.1</td><td>  </td></tr>
<tr><td> dt330 </td><td> kernel 3.1.10-1.19-default on openSuSE 12.1</td><td>  </td></tr>
<tr><td> nudaq_driver </td><td> TBD </td><td> as of kernel 2.6.13 or so, this thing hangs unpredictably after a few minutes activity.</td></tr>
<br>
<tr><td> owis2 </td><td> kernel 2.6.27.25-0.1 SMP on x86_64, Opensuse 11.1 </td><td>  </td></tr>
<tr><td> USB2000 </td><td> kernel 2.6.22.19 on x86_64, OpenSuSE 10.3<br>kernel 2.6.24-23-generic on i686, Xubuntu 7.10<br>kernel 2.6.25.20 on i686, OpenSuSE 11.0<br>kernel 2.6.31.12-0.1-desktop on Suse 11.2 as of svn rev 37<br>kernel 3.7.10-1.1-desktop on openSUSE12.3</td><td> some ioctl changes with svn rev 37,<br>new udev rule with svn-58 </td></tr>