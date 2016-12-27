import os
import signal
import sys
import subprocess
import time

import testing
import testify as T

class ProcessManagementTestCase(testing.BaseTestCase):

    def test_child(self):
        """catbox.run will fork(). Child process will execute
        self.child_function. This test verifies that child is actually
        running the given function reporting back to parent through a
        Unix pipe.
        """
        self.run_child_function_in_catbox()
        self.verify_message_from_child()

    def test_child_does_not_report_back(self):
        def lazy_child():
            pass

        with T.assert_raises(testing.ChildDidNotReportBackException):
            # lazy_child function will not report anything back and
            # parent should timeout waiting for it.
            self.run_child_function_in_catbox(lazy_child)
            self.verify_message_from_child()


    def test_subprocess_kill(self):
        """Verify that killing the subprocess in the forked process
        will not have side effects on catbox.
        """
        def child_calling_subprocess_kill():
            sleep_time = 5
            start_time = time.time()
            sub = subprocess.Popen(['/bin/sleep', '%d' % sleep_time], stdout=subprocess.PIPE)
            sub.kill()
            sub.wait()
            elapsed_time = time.time() - start_time
            assert elapsed_time < sleep_time
            os.write(self.write_pipe, self.default_expected_message_from_child)

        self.run_child_function_in_catbox(child_function=child_calling_subprocess_kill)
        self.verify_message_from_child()


class WatchdogTestCase(testing.BaseTestCase):

    def test_watchdog(self):
        def sleeping_child_function():
            time.sleep(5)

        child_processes = []

        # To test watchdog we'll kill the parent and see if the
        # watchdog takes care of killing the child process. We'll fork
        # here and run catbox in the forked process to kill the catbox
        # parent prematurely.
        catbox_pid = os.fork()
        if not catbox_pid: # catbox process
            self.run_child_function_in_catbox(
                child_function=sleeping_child_function
            )
            assert False, "Shouldn't get here. Parent should kill us already."
            sys.exit(0)
        else:
            # wait for catbox and traced child to start up and
            # initialized. We're just sleeping enough for catbox
            # process to have a chance to start running.
            time.sleep(.1)

            child_processes = testing.child_processes(catbox_pid)

            # Killing catbox (parent) will trigger watchdog process
            # and it will kill the process group.
            os.kill(catbox_pid, signal.SIGKILL)
            os.waitpid(catbox_pid, 0) # block waiting catbox_pid

        # testing.is_process_alive sends NULL signal to catbox
        # process. Although we kill the catbox process and wait for it
        # we were still able to send the signal. Waiting for a very
        # short amount of time helps with "process cleanup".
        time.sleep(.1)
        T.assert_equal(
            testing.is_process_alive(catbox_pid),
            False,
            "Catbox process (%d) is still running" % catbox_pid
        )

        for pid in child_processes:
            pid = int(pid)
            T.assert_equal(
                testing.is_process_alive(pid),
                False,
                "Child process (%d) is still running" % pid
            )
