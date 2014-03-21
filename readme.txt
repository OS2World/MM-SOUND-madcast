
 * MadCast 0.0.1 *


 Intro
 -----

 I have mp3-station in our local network and many VBR-encoded mp3-files (suxx!).
 I'm very lazy to re-encode it as normal CBR mp3's and this is main reason why
 i had to wrote this little program.
 

 What it does
 ------------

 As well-known 'encoder' shout (included in icecast package), it takes an number
 of mp3-files as playlist and sends they as stream into icecast server input.

 Unlike shout, it also does some odd things such as parsing mpeg stream frames
 (like normal mp3-player), and this makes it working correctly with VBR-encoded
 mp3's.


 How to use
 ----------

 MadCast have several command-line parameters:

 <hostname[:port]>      Host on which IceCast will listen and port to connect
                        Default port is 8001
 <password>             IceCast password
 <playlist>             Playlist filename (in standart format)
 [-r]                   Shuffle playlist
 [-l]                   Loop playlist
 [-b msec]              Specify playing buffer length in milliseconds
                        (125, 250 and 500 are really good values)
                        Low limit is 100 and high limit is 10000

 [] idicate optional parameters and <> indicate obligatory parameters


 Example: madcast localhost:8001 Pwd4MyC00lRadio g:\music\mp3\mad_radio.lst -r -l -b 125

 License
 -------

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR OR CONTRIBUTORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.


 Requrements
 -----------

 OS/2 Warp 3.0 or higher (tested with FP 43)
 TCP/IP 4.0 (yes, it works with old 16-bit stacks)
 IceCast :)


 Contact
 -------

 real name: Dmitry Zaharov
 e-mail: madint@os2.ru
 irc: EfNet/#os2russian/[MadInt] or MadInt_W3
