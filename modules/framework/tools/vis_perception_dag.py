"""
File: vis_log.py
Brief: This module is used to vis the dag file
Author: Feng DING
Date: 2022/01/05 15:24:00
Last modified: 2022/01/05 15:24:00
"""

import os
import sys
from argparse import ArgumentParser
from google.protobuf import text_format
sys.path.append(os.path.join((os.path.dirname(os.path.abspath(__file__))), "..", "python"))
import dag_config_pb2
import pydot


OP_STYLE_DEFAULT = {'shape': 'record',
                    'fillcolor': '#AECA42',
                    'style': 'filled'}

OP_STYLE_BYPASS = {'shape': 'record',
                   'fillcolor': '#6495ED',
                   'style': 'dashed'}

BLOB_STYLE = {'shape': 'octagon',
              'fillcolor': '#E0E0E0',
              'style': 'filled'}

STRATEGY_STYPE = {'shape': 'diamond',
                  'fontcolor': '#E1E5E7',
                  'fillcolor': '#245A7F',
                  'style': 'filled'}


def get_op_label(op, rankdir):
    alist = []
    if op.algorithm:
        alist.append({'algorithm': op.algorithm, 'bypass': False})
    elif op.group:
        for o in op.group.op:
            alist.append({'algorithm': o.algorithm, 'bypass': o.bypass})

    rows = []
    row = '<font color="#3580A4">{}</font>'.format(op.name)
    row = '<tr><td bgcolor="#F1F2F3">{}</td></tr>'.format(row)
    rows.append(row)
    for a in alist:
        color = "#275D16"
        if a['bypass']:
            color = "#757575"
        row = '<font color="{0}">* {1}</font>'.format(color, a['algorithm'])
        row = '<tr><td>{}</td></tr>'.format(row)
        rows.append(row)
    prop = 'border="0" cellborder="1" cellspacing="0" cellpadding="4"'
    table = '<table {0}>{1}</table>'.format(prop, '\n'.join(rows))
    html = '<{}>'.format(table)
    return html


def get_output_label(top_blob, rankdir):

    separator = r'\n'

    descriptors_list = []

    if len(top_blob.event) > 0:
        descriptors_list.append("event: "+top_blob.event)
    if len(top_blob.data) > 0:
        descriptors_list.append("data: "+top_blob.data)
    if len(top_blob.type) > 0:
        descriptors_list.append("type: "+top_blob.type)
    if top_blob.hz:
        descriptors_list.append("hz: "+str(top_blob.hz))

    node_label = separator.join(descriptors_list)
    node_label = '"%s"' % node_label

    return node_label, len(descriptors_list)


def choose_color_by_output(node_label):
    if node_label == 1:
        color = '#E0E0E0'
    else:
        color = '#CC33FF'
    return color


def get_pydot_graph(dagnet, rankdir, label_edges=True):

    pydot_graph = pydot.Dot('DagNet', graph_type='digraph', rankdir=rankdir)
    pydot_nodes = {}
    pydot_edges = []

    strategy_nodes = {}
    for op in dagnet.op:
        # node name represents the op node
        # generate the op name and op algorithm
        node_label = get_op_label(op, rankdir)
        node_name = "%s_%s" % (op.name, op.algorithm)

        op_style = OP_STYLE_DEFAULT

        if op.bypass:
            op_style = OP_STYLE_BYPASS

        pydot_nodes[node_name] = pydot.Node(name=node_name,
                                            label=node_label, **op_style)

        for bottom_blob in op.trigger:
            if bottom_blob+"_blob" not in pydot_nodes:
                pydot_nodes[bottom_blob+"_blob"] = \
                    pydot.Node('%s' % bottom_blob, **BLOB_STYLE)
            edge_style = {'label': '"trigger"', 'style': 'filled'}
            pydot_edges.append({'src': bottom_blob + '_blob',
                                'dst': node_name,
                                'label': edge_style
                                })

        if len(op.input) > 0:
            for ind, input_blob in enumerate(op.input):
                if input_blob+"_blob" not in pydot_nodes:
                    pydot_nodes[input_blob + "_blob"] = \
                        pydot.Node('%s' % input_blob, **BLOB_STYLE)
                if len(op.input_offset) > ind:
                    offset = op.input_offset[ind]
                else:
                    offset = 0

                edge_style = {'label': '"offset"'+r'\n'+str(offset)+r'\n',
                              'style': 'dashed'}
                pydot_edges.append({'src': input_blob + '_blob',
                                    'dst': node_name, 'label': edge_style})

        if len(op.latest) > 0:
            for ind, latest_blob in enumerate(op.latest):
                blob_name = latest_blob + "_blob"
                if blob_name not in pydot_nodes:
                    pydot_nodes[blob_name] = \
                        pydot.Node('%s' % latest_blob, **BLOB_STYLE)
                edge_style = {'label': '"latest"', 'fillcolor': '#FF5050',
                              'color': '#FF5050',
                              'style': 'dashed'}
                pydot_edges.append({'src': blob_name,
                                    'dst': node_name, 'label': edge_style})

        for top_blob in op.output:
            if top_blob.event:
                # if top_blob.event + '_blob' not in pydot_nodes:
                output_label, output_size = get_output_label(top_blob, rankdir)
                output_style = BLOB_STYLE.copy()
                output_style['fillcolor'] = choose_color_by_output(output_size)
                pydot_nodes[top_blob.event + '_blob'] = \
                    pydot.Node(output_label, **output_style)
                top_name = top_blob.event+'_blob'
            elif top_blob.hz and str(top_blob.hz) + '_blob' not in pydot_nodes:
                output_label, output_size = get_output_label(top_blob, rankdir)
                output_style = BLOB_STYLE.copy()
                output_style['fillcolor'] = '#FF9900'
                pydot_nodes[str(top_blob.hz)+'_blob'] = \
                    pydot.Node(output_label, **output_style)
                top_name = str(top_blob.hz)+'_blob'
            edge_style = {'label': '"output"', 'style': 'filled'}
            pydot_edges.append({'src': node_name,
                                'dst': top_name,
                                'label': edge_style,
                                })

    for node in pydot_nodes.values():
        pydot_graph.add_node(node)

    for edge in pydot_edges:
        pydot_graph.add_edge(
            pydot.Edge(pydot_nodes[edge['src']],
                       pydot_nodes[edge['dst']],
                       **edge['label']))
    return pydot_graph


def draw_dagnet_to_file(net, filename, rankdir='LR'):
    ext = filename[filename.rfind('.')+1:]
    with open(filename, 'wb') as fid:
        fid.write(draw_dagnet(net, rankdir, ext))


def draw_dagnet(net, rankdir, ext='png'):
    return get_pydot_graph(net, rankdir).create(format=ext)


def parse_args():
    parser = ArgumentParser()
    parser.add_argument('--input_proto_file',
                        '-i', type=str,
                        help='Input dag prototxt file')
    parser.add_argument('--output_image_file', '-o',
                        type=str, help='Output image file')
    parser.add_argument('--rankdir', '-r', type=str,
                        help=('One of TB (top-bottom, i.e., vertical), '
                              'RL (right-left, i.e., horizontal), or another'
                              'valid dot option; see'
                              'http://www.graphviz.org/doc/info/'
                              'attrs.html#:rankdir'),
                        default='TB')

    args = parser.parse_args()
    return args


def main():
    args = parse_args()
    net = dag_config_pb2.DAGConfig()
    try:
        text_format.Merge(open(args.input_proto_file).read(), net)
    except:
        print("Please enter the correct input_proto_file!!!")
        sys.exit()

    print('Drawing net to %s' % args.output_image_file)
    draw_dagnet_to_file(net, args.output_image_file, args.rankdir)


if __name__ == '__main__':
    main()
