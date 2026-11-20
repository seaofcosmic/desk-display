import os, pty, subprocess, sys, shutil, tempfile, time
here = os.path.dirname(os.path.abspath(__file__))
comp = '/home/claude/desk-display/companion'
master, slave = pty.openpty()
import tty; tty.setraw(master); tty.setraw(slave)
data = tempfile.mkdtemp()
shutil.copy(os.path.join(comp, 'reminders.txt'), data)
open(os.path.join(data, 'settings.json'), 'w').write('{"city": "Culver City"}')
sim = subprocess.Popen([os.path.join(here, 'sim_set'), str(master)], pass_fds=[master], cwd=here, stderr=open(os.path.join(here, 'device.log'), 'w'))
time.sleep(0.3)
co = subprocess.Popen([sys.executable, os.path.join(comp, 'desk_display.py'), '--demo', '--port', os.ttyname(slave), '--data-dir', data, '--verbose'],
                      stdout=open(os.path.join(here, 'companion.log'), 'w'), stderr=subprocess.STDOUT)
sim.wait()
co.terminate()
print('sim exit', sim.returncode)
