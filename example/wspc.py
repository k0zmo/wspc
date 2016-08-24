from __future__ import print_function
from ws4py.client.threadedclient import WebSocketClient
from queue import Queue
import json

class JsonSerializable(object):
    def to_JSON(self):
        return json.dumps(self, 
            default=lambda o: o.__dict__,
            sort_keys=True, indent=4)

class Request(JsonSerializable):
    id = 0

    def __init__(self, method, *args, **kwargs):
        Request.id += 1
        self.id = Request.id
        self.method = method
        self.params = args if args else kwargs

class EventRegistry:
    class EventCallback:
        def __init__(self, cb, one_shot):
            self.cb = cb
            self.one_shot = one_shot

    def __init__(self):
        self.cbs = {}

    def __getattr__(self, name):
        def wrapper(cb, one_shot=False):
            self._register_event(name, cb, one_shot)
        return wrapper

    def _register_event(self, name, cb, one_shot):
        if not name in self.cbs:
            self.cbs[name] = EventRegistry.EventCallback(cb, one_shot)

    def call(self, name, **kwargs):
        if name in self.cbs:
            ret = self.cbs[name].cb(**kwargs)
            if self.cbs[name].one_shot and ret:
                del self.cbs[name]

class MessageValidator:
    @staticmethod
    def is_err(msg):
        return 'error' in msg

    @staticmethod
    def is_response(msg):
        return 'result' in msg and \
               not 'error' in msg and \
               'id' in msg

    @staticmethod
    def is_notif(msg):
        return 'method' in msg and 'params' in msg

class Client(WebSocketClient):
    def __init__(self, address):
        super(Client, self).__init__(address)
        self.event_registry = EventRegistry()
        self.req_id = []
        self.queue = Queue(1)
        self.connect()

    def _event_received(self, event_name, args):
        self.event_registry.call(event_name, **args)

    def received_message(self, m):
        try:
            resp = json.loads(str(m))
            if MessageValidator.is_err(resp):
                raise Exception(resp['error'])
            if MessageValidator.is_response(resp):
                # Is this response on our list of pending ones?
                if int(resp['id']) in self.req_id:
                    # Put the response dict onto the queue 
                    # and remove its ID from pending list
                    self.queue.put(resp['result'])
                    self.req_id.remove(int(resp['id']))
            elif MessageValidator.is_notif(resp):
                # Notify of inbound event from the server
                self._event_received(resp['method'], resp['params'])
        except Exception as e:
            print('Invalid response:', str(m), e)
            self.queue.put(e)

    def closed(self, code, reason=None):
        print('Closed down: {}, {}'.format(code, reason))
        self.queue.put(None)

    @property
    def callbacks(self):
        return self.event_registry

    @property
    def event(self):
        return self.event_registry

    def __getattr__(self, name):
        def func_wrapper(*args, **kwargs):
            # Create a Request object and enqueued its ID so
            # we can associate request with the response 
            request = Request(name, *args, **kwargs)
            self.req_id.append(request.id)
            # Send as a JSON
            self.send(request.to_JSON())
            # Wait (blocking) for response with the same ID
            return self.queue.get(True)
        return func_wrapper
