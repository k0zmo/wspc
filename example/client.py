import wspc

try:
    c = wspc.Client('ws://localhost:9001')
    '''
    Go to http://localhost:9001 to get list of supported remote procedures
    and notifications along with their arguments
    '''

    c.callbacks.ping_event(
        lambda tick: print('ping - tick: {}'.format(tick)),
        one_shot=False)

    ping_response = c.ping();
    print('ping() response: {}'.format(ping_response['tick']))

    add_resp = c.calculate(arg1=20, arg2=5, op='add', comment='adding 20 to 5')
    print('calculate(20, 5, ''add'') response: {}'.format(add_resp))

    divide_resp = c.calculate(arg1=16, arg2=5, op='divide')
    print('calculate(16, 5, ''divide'') response: {}'.format(divide_resp))

    # Wrong function
    c.calculate2()

    # wait for events
    c.run_forever()

except KeyboardInterrupt:
    c.close()
