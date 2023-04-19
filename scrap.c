int main(int argc, char **argv)
{
    init();

    if (argc > 1)
    {
        if (Fork() == 0)
        {
            Exec(argv[1], argv + 1);
        }
        else
        {
            while (true)
            {

                int ret;

                struct m_template tmp_received;

                int received_pid = Receive(&tmp_received);

                if (received_pid == ERROR)
                {
                    TracePrintf(1, "ERROR: MESSAGE NOT RECEIVED, SHUTTING DOWN YFS \n");
                    handle_shutdown();
                }

                printf("Message received\n");
                int tmp = tmp_received.num;

                switch (tmp)
                {
                case YFS_OPEN:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_open(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_CREATE:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_create(pathname, message->cur_i, -1);
                    free(pathname);
                    break;
                }

                case YFS_READ:
                {
                    struct m_file *message = (struct m_file *)&tmp_received;
                    ret = handle_read(message->i_num, message->buf, message->size, message->offset, received_pid);
                    break;
                }

                case YFS_WRITE:
                {
                    struct m_file *message = (struct m_file *)&tmp_received;
                    ret = handle_write(message->i_num, message->buf, message->size, message->offset, received_pid);
                    break;
                }

                case YFS_SEEK:
                {
                    struct m_seek *message = (struct m_seek *)&tmp_received;
                    ret = handle_seek(message->i_num, message->offset, message->whence, message->cur_pos);
                    break;
                }

                case YFS_LINK:
                {
                    struct m_link *message = (struct m_link *)&tmp_received;
                    char *old_name = get_proc_path(received_pid, message->old_name, message->old_len);
                    char *new_name = get_proc_path(received_pid, message->new_name, message->new_len);
                    ret = handle_link(old_name, new_name, message->cur_i);
                    free(old_name);
                    free(new_name);
                    break;
                }

                case YFS_UNLINK:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_unlink(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_MKDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_mkdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_RMDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_rmdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_CHDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_chdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_STAT:
                {
                    struct message_stat *message = (struct message_stat *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_stat(pathname, message->cur_i, message->stat_buffer, received_pid);
                    free(pathname);
                    break;
                }

                case YFS_SYNC:
                {
                    ret = handle_sync();
                    break;
                }

                case YFS_SHUTDOWN:
                {
                    ret = handle_shutdown();
                    break;
                }

                default:
                {
                    TracePrintf(1, "Invalid Operation %d\n", tmp_received.num);
                    ret = ERROR;
                    break;
                }

                }

                // send reply
                struct m_template msg_rply;
                msg_rply.num = ret;
                
                if (Reply(&msg_rply, received_pid) != 0)
                {
                    TracePrintf(1, "ERROR: unable to reply to user operation received_pid: %d\n", received_pid);
                }
            }
        }
    }

    return 0;
}