import contextlib
import errno
import os
import select
import subprocess
import sys

import testify as T

import catbox

class ChildDidNotReportBackException(Exception):
    pass


@contextlib.contextmanager
def no_stderr():
    orig_stderr = sys.stderr
    with open("/dev/null", "w") as null:
        sys.stderr = null
        yield
    sys.stderr = orig_stderr


def is_process_alive(pid):
    """Sends null signal to a process to check if it's alive"""
    try:
        # Sending the null signal (sig. 0) to the process will check
        # pid's validity.
        os.kill(pid, 0)
    except OSError, e:
        # Access denied, but process is alive
        return e.errno == errno.EPERM
    except:
        return False
    else:
        return True

def child_processes(parent_pid):
    ps = subprocess.Popen(
        "ps -o pid --ppid %d --noheaders" % parent_pid,
        shell=True,
        stdout=subprocess.PIPE
    )
    out = ps.stdout.read()
    ps.wait()
    return out.split()


class BaseTestCase(T.TestCase):
    __test__ = False

    @T.setup
    def prepare_parent_child_communication(self):
        self.MAX_READ_SIZE = 1024
        # Create a pipe to verify that child is created and can
        # communicate with the parent.
        self.read_pipe, self.write_pipe = os.pipe()
        self.default_expected_message_from_child = "I'm ALIVE!"

        self.epoll = select.epoll()
        self.epoll.register(self.read_pipe, select.EPOLLIN | select.EPOLLET)

        def child_function():
            os.close(self.read_pipe)
            os.write(self.write_pipe, self.default_expected_message_from_child)

        self.default_child_function = child_function

    @T.teardown
    def close_pipe(self):
        try:
            os.close(self.read_pipe)
        except OSError:
            pass
        try:
            os.close(self.write_pipe)
        except OSError:
            pass

    def poll(self):
        events = self.epoll.poll(.1)
        if events:
            first_event = events[0]
            read_fd = first_event[0]
            return os.read(read_fd, self.MAX_READ_SIZE)
        return False

    def verify_message_from_child(self, expected_message=None):
        expected_message = expected_message or self.default_expected_message_from_child
        actual_message_from_child = self.poll()
        if actual_message_from_child:
            T.assert_in(expected_message, actual_message_from_child)
        else:
            raise ChildDidNotReportBackException

    def run_child_function_in_catbox(self, child_function=None, event_hooks=None):
        child_function = child_function or self.default_child_function
        catbox.run(child_function, event_hooks=event_hooks)
