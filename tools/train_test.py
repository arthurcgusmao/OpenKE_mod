import logging
import config
import models
import json
import numpy as np
import pandas as pd
import os
import time
import multiprocessing
from ctypes import c_float # for retrieving test results
from itertools import product # for grid_search function
from plots import plot_learning_curve

def setup_config(model_info):
    """Create config instance and set variables/hyperparameters.
    """
    con = config.Config()
    dataset_path = "./benchmarks/{}/".format(model_info['dataset_name'])
    con.set_in_path(dataset_path)
    if 'test_link_prediction' in model_info: con.set_test_link_prediction(model_info['test_link_prediction'])
    if 'test_link_prediction' in model_info: con.set_test_triple_classification(model_info['test_triple_class'])
    con.set_log_on(model_info['log_on'], log_type=model_info['log_type'], log_print=model_info['log_print'])
    con.set_train_times(model_info['n_epochs'])
    if 'batch_size' in model_info:
        con.set_batch_size(model_info['batch_size'])
    elif 'n_batches' in model_info:
        con.set_nbatches(model_info['n_batches'])
    else:
        raise ValueError('Either `batch_size` or `n_batches` should be defined in `model_info`.')
    con.set_alpha(model_info['learning_rate'])
    con.set_bern(model_info['bern'])
    con.set_dimension(model_info['k'])
    if 'margin' in model_info: con.set_margin(model_info['margin'])
    if 'regul_weight' in model_info: con.set_lmbda(model_info['regul_weight'])
    if 'score_norm' in model_info: con.score_norm = model_info['score_norm']
    if 'ent_neg_rate' in model_info: con.set_ent_neg_rate(model_info['ent_neg_rate'])
    if 'rel_neg_rate' in model_info: con.set_rel_neg_rate(model_info['rel_neg_rate'])
    con.set_opt_method(model_info['opt_method'])
    if 'shuffle' in model_info: con.shuffle = model_info['shuffle']
    if 'work_threads' in model_info: con.set_work_threads(model_info['work_threads'])
    return con


def read_model_info(import_path):
    """Reads `model_info.tsv` from a model directory path and returns its information in a dict.
    """
    model_info_df = pd.read_csv('{}/model_info.tsv'.format(import_path), sep='\t')
    # transform model info into dict with only one "row"
    model_info = model_info_df.to_dict()
    for key,d in model_info.iteritems():
        model_info[key] = d[0]
    # add timestamp to model if not present
    if not 'timestamp' in model_info:
        model_info['timestamp'] = os.path.abspath(import_path).split(os.sep)[-1]
    return model_info


def restore_model(import_path):
    """Restores the whole model from a results folder using information that was saved in
    `model_info.tsv` and `tf_model`, following the setup in `pipeline()`.
    """
    model_info = read_model_info(import_path)

    # setup config instance
    con = setup_config(model_info)
    con.init()
    con.set_model_by_name(model_info['model_name'])
    con.import_variables("{}/tf_model/model.vec.tf".format(import_path)) # loading model via tensor library
    return con


def pipeline(model_info):
    """Train and tests an embedding model using the parameters supplied.
    """

    # Define Variables
    # ----------------

    ### main variables
    # model_info['dataset_name'] = "FB13"
    # model_info['model_name']   = "TransE"

    ### necessary hyperparameters
    # model_info['batch_size']    = 2048 # mini-batch size (takes precedence over n_batches)
    # model_info['n_batches']     = None # number of batches
    # model_info['n_epochs']      = 1000 # epochs
    # model_info['learning_rate'] = 0.001
    # model_info['k']             = 100 # embedding dimension
    # model_info['opt_method']    = 'adam'

    ### optional or model dependent hyperparameters
    # model_info['margin']                   = 1.0  # for most models
    # model_info['score_norm']               = 'l2' # for TransX models
    # model_info['embedding_initialization'] = 'TransE/1524490825' # initialize parameters from another model
    # model_info['regul_weight']             = 0.001 # the regularization weight in the loss function

    ### negative examples and training settings
    if not 'ent_neg_rate' in model_info:  model_info['ent_neg_rate'] = 1
    if not 'rel_neg_rate' in model_info:  model_info['rel_neg_rate'] = 0
    if not 'bern'         in model_info:  model_info['bern']         = 1 # Bernoulli distribution for generating negative training examples
    if not 'shuffle'      in model_info:  model_info['shuffle']      = 1 # shuffle training set (each epoch) instead of randomly sampling from it

    ### test settings
    if not 'test_link_prediction' in model_info:  model_info['test_link_prediction'] = True
    if not 'test_triple_class'    in model_info:  model_info['test_triple_class']    = True

    ### logging settings
    if not 'log_on'    in model_info:  model_info['log_on']    = 1
    if not 'log_type'  in model_info:  model_info['log_type']  = 'epoch'
    if not 'log_print' in model_info:  model_info['log_print'] = True

    ### GPU and CPU settings
    if not 'work_threads' in model_info:  model_info['work_threads'] = multiprocessing.cpu_count()
    if 'cuda_device' in model_info: os.environ["CUDA_VISIBLE_DEVICES"] = str(model_info['cuda_device']) # has to be a string

    print "--- STARTING PIPELINE ---\n\n{}".format(model_info)

    con = setup_config(model_info)


    # Training
    # --------
    print "\n--- TRAINING ---\n"
    # get model
    embedding_model = getattr(models, model_info['model_name'])
    # define export path
    export_path = "./results/{}/{}/{}".format(
        model_info['dataset_name'],
        model_info['model_name'],
        int(time.time()))
    # create export_path dir
    def ensure_dir(file_path):
        directory = os.path.dirname(file_path)
        if not os.path.exists(directory):
            os.makedirs(directory)
    ensure_dir(export_path + '/') # without the "/" in the end the path wasn't being created
    print("Model will be exported to {}. \n".format(export_path))
    def save_model_info():
        pd.DataFrame([model_info]).to_csv('{}/model_info.tsv'.format(export_path), sep='\t')

    # export model parameters
    con.set_export_files("{}/tf_model/model.vec.tf".format(export_path))
    # con.set_out_files("{}/embedding.vec.json".format(export_path))

    con.init()
    con.set_model(model=embedding_model)
    if 'embedding_initialization' in model_info: con.set_parameters_from_json("./results/{}/{}/embedding.vec.json".format(model_info['dataset_name'], model_info['embedding_initialization']))
    con.run()

    # logging
    learning_curve = pd.DataFrame(con.log['training_curve'])
    learning_curve.to_csv("{}/learning_curve.tsv".format(export_path), sep='\t')
    model_info['learning_time'] = con.log['learning_time']
    save_model_info()
    print('\nModel was trained in {} seconds'.format(model_info['learning_time']))


    # Plot/Save the learning curve
    # ----------------------------
    plot_learning_curve(learning_curve, model_info, export_path)


    # Testing
    # -------
    print "\n--- TESTING ---\n"
    con.test()

    if model_info['test_link_prediction']: # link prediction
        model_info['mrr_raw'] = c_float.in_dll(con.lib, 'mrr_raw').value
        model_info['mr_raw'] = c_float.in_dll(con.lib, 'mr_raw').value
        model_info['hits_10_raw'] = c_float.in_dll(con.lib, 'hits_10_raw').value
        model_info['hits_3_raw'] = c_float.in_dll(con.lib, 'hits_3_raw').value
        model_info['hits_1_raw'] = c_float.in_dll(con.lib, 'hits_1_raw').value
        model_info['mrr_filter'] = c_float.in_dll(con.lib, 'mrr_filter').value
        model_info['mr_filter'] = c_float.in_dll(con.lib, 'mr_filter').value
        model_info['hits_10_filter'] = c_float.in_dll(con.lib, 'hits_10_filter').value
        model_info['hits_3_filter'] = c_float.in_dll(con.lib, 'hits_3_filter').value
        model_info['hits_1_filter'] = c_float.in_dll(con.lib, 'hits_1_filter').value

    if model_info['test_triple_class']: # triple classification
        model_info['acc'] = c_float.in_dll(con.lib, 'aveAcc').value
        print "\nAccuracy in test set is {}\n".format(model_info['acc'])

    model_info['testing_time'] = con.log['testing_time']
    print('Model was tested in {} seconds'.format(model_info['testing_time']))
    save_model_info()



def grid_search(param_grid):
    """Grid search. Uses the same `param_grid` interface as
    `[CV in sklearn](http://scikit-learn.org/stable/modules/grid_search.html#exhaustive-grid-search)`.
    """
    # adapt to list if only one dict is passed
    if type(param_grid) == dict:
        param_grid = [param_grid]

    # adapt each value of each grid to list if necessary
    for grid in param_grid:
        for key,val in grid.iteritems():
            if type(val) != list:
                grid[key] = [val]

    # run the pipeline for each model configuration
    g = 1
    for grid in param_grid:
        models = list(product(*grid.values()))
        params_names = grid.keys()
        c = 1
        for model in models:
            model_info = {}
            for i in range(len(params_names)):
                model_info[params_names[i]] = model[i]
            print "\n /################################################################\\"
            print   "< ###################  Grid {}/{} | Search {:2d}/{:<2d} ################### >".format(g, len(param_grid), c, len(models))
            print   " \################################################################/\n"
            pipeline(model_info)
            c += 1
        g += 1
