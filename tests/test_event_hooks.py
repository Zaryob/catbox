import os
import time

import testing
import testify as T

import catbox


class EventHooksTestCase(testing.BaseTestCase):
    def test_non_dictionary_event_hooks(self):
        self.run_child_function_in_catbox(event_hooks=None)

    def test_invalid_event_hooks(self):
        event_hooks = {"child_initialized_hook" : "invalid_event_hook"}
        with T.assert_raises(TypeError):
            self.run_child_function_in_catbox(event_hooks=event_hooks)

    def test_successful_child_initialized_hook(self):
        expected_message_from_init_function = "Init function is successful!"
        def child_initialized_hook(child_pid):
            os.write(self.write_pipe, expected_message_from_init_function)

        event_hooks = {"child_initialized" : child_initialized_hook}
        self.run_child_function_in_catbox(event_hooks=event_hooks)
        self.verify_message_from_child(expected_message_from_init_function)

    def test_failing_child_initialized_hook(self):
        def child_initialized_hook(child_pid):
            raise Exception, "child_initialized hook raises exception"

        # When child_initialized hook fails parent process will
        # exit. To test a failing initilization hook we fork and watch
        # the new child.
        pid = os.fork()
        if not pid:
            event_hooks = {"child_initialized" : child_initialized_hook}
            with testing.no_stderr():
                # This will fail. redirecting stderr to /dev/null will
                # silence the test output.
                self.run_child_function_in_catbox(event_hooks=event_hooks)
        else:
            status = 0
            wait_pid = 0
            try:
                for _ in range(5):
                    (wait_pid, status, _) = os.wait4(pid, os.WNOHANG)
                    if wait_pid == pid:
                        break
                    time.sleep(.1)
            except OSError, e:
                T.assert_in("No child processes", e)
            else:
                T.assert_not_equal(
                    status,
                    0,
                    "Failing child_initialized hook did not make parent exit"
                )

    def test_catbox_run_with_no_event_hooks(self):
        catbox.run(self.default_child_function)
